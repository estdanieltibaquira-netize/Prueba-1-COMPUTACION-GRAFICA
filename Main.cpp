#include<iostream>
#include<fstream>
#include<sstream>
#include<vector>
#include<string>
#include<cmath>
#include<algorithm>
#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>

//configurar shaders de OpenGL
const char* vertexShaderSource = "#version 330 core\n"
"layout (location = 0) in vec3 aPos;\n" //posicion (x,y,z) de cada vertice
"layout (location = 1) in vec3 aNormal;\n" //vector normal (nx,ny,nz) de cada vertice: es perpendicular a la superficie y sirve para calcular la luz
"out vec3 FragPos;\n" //posicion del vertice en el mundo (se la mandamos al fragment shader)
"out vec3 Normal;\n" //normal ya transformada al mundo (tambien se la mandamos al fragment shader)
"uniform mat4 model;\n"
"uniform mat4 view;\n"
"uniform mat4 projection;\n"
"uniform mat3 normalMatrix;\n" //matriz especial para las normales (la calculamos en C++, como recomiendan las diapositivas)
"void main()\n"
"{\n"
"   FragPos = vec3(model * vec4(aPos, 1.0));\n" //solo multiplicamos por model para quedar en coordenadas del mundo (los calculos de luz se hacen en el mundo)
"   Normal = normalMatrix * aNormal;\n" //la normal NO se multiplica por model, se usa la matriz de normales
"   gl_Position = projection * view * vec4(FragPos, 1.0);\n" //model -> view -> projection
"}\0";

const char* fragmentShaderSource = "#version 330 core\n"
"out vec4 FragColor;\n" //color final del pixel
"in vec3 FragPos;\n" //posicion del fragmento en el mundo (llega interpolada desde el vertex shader)
"in vec3 Normal;\n" //normal del fragmento (tambien interpolada)
"uniform vec3 colorBase;\n" //color del objeto
"uniform float usarLuz;\n" //1.0 = aplicar iluminacion (helicoptero), 0.0 = color plano (la curva y la lampara)
"uniform vec3 lightPos;\n" //posicion de la luz en el mundo
"uniform vec3 lightColor;\n" //color de la luz
"uniform vec3 viewPos;\n" //posicion de la camara en el mundo (la necesita la luz especular)
"void main()\n"
"{\n"
"   if (usarLuz < 0.5)\n"
"   {\n"
"      FragColor = vec4(colorBase, 1.0);\n" //sin luz: se pinta del color plano y se termina
"      return;\n"
"   }\n"
"   vec3 norm = normalize(Normal);\n" //siempre hay que normalizar los vectores en los calculos de luz (error comun si se olvida)
"   vec3 viewDir = normalize(viewPos - FragPos);\n" //vector que va del fragmento hacia la camara
"   if (dot(norm, viewDir) < 0.0) norm = -norm;\n" //si la normal apunta al lado contrario de la camara la volteamos (asi no se oscurecen las caras con orientacion rara)
"   float ambientStrength = 0.2;\n" //que tan fuerte es la luz ambiental
"   vec3 ambient = ambientStrength * lightColor;\n" //LUZ AMBIENTAL: un poquito de luz constante para que nada quede totalmente negro
"   vec3 lightDir = normalize(lightPos - FragPos);\n" //vector que va del fragmento hacia la luz
"   float diff = max(dot(norm, lightDir), 0.0);\n" //producto punto entre la normal y la luz; max evita valores negativos (caras de espaldas a la luz)
"   vec3 diffuse = diff * lightColor;\n" //LUZ DIFUSA: mas brillo mientras mas de frente le llega la luz a la cara
"   float specularStrength = 0.5;\n" //que tan fuerte es el reflejo
"   vec3 reflectDir = reflect(-lightDir, norm);\n" //direccion en la que rebota la luz (reflect espera el vector de la luz hacia el fragmento, por eso el menos)
"   float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);\n" //que tanto coincide el rebote con la mirada; 32 es el brillo (mas alto = punto de luz mas pequeno)
"   vec3 specular = specularStrength * spec * lightColor;\n" //LUZ ESPECULAR: el punto brillante, del color de la luz
"   vec3 result = (ambient + diffuse + specular) * colorBase;\n" //se suman las tres luces y se multiplican por el color del objeto
"   FragColor = vec4(result, 1.0);\n"
"}\n\0";


// ---- CURVA DE BEZIER CUBICA ----
// Se define con 4 puntos: P0 (inicio), P1 y P2 (puntos de control que "jalan" la curva) y P3 (final)
// B(t) = (1-t)^3 * P0 + 3(1-t)^2 * t * P1 + 3(1-t) * t^2 * P2 + t^3 * P3 , con t entre 0 y 1
glm::vec3 bezier(const glm::vec3* P, float t)
{
	float u = 1.0f - t;
	return u * u * u * P[0] + 3.0f * u * u * t * P[1] + 3.0f * u * t * t * P[2] + t * t * t * P[3];
}

// La derivada de la curva da la direccion hacia donde "avanza" en cada punto (para que el helicoptero mire hacia adelante)
glm::vec3 bezierTangente(const glm::vec3* P, float t)
{
	float u = 1.0f - t;
	return 3.0f * u * u * (P[1] - P[0]) + 6.0f * u * t * (P[2] - P[1]) + 3.0f * t * t * (P[3] - P[2]);
}


// Una "parte" del modelo = un objeto/grupo del OBJ
struct Parte
{
	std::string nombre;
	GLuint VAO = 0, VBO = 0;
	int cantidad = 0; //cuantos vertices tiene
	glm::vec3 minimo = glm::vec3(0.0f); //esquina mas chica de la parte (sirve para calcular el centro de las helices)
	glm::vec3 maximo = glm::vec3(0.0f); //esquina mas grande de la parte
};

// Dice si el nombre de una parte CONTIENE una palabra, sin importar mayusculas o minusculas
// (asi funciona aunque Maya le agregue cosas al nombre, por ejemplo "heliceGrande_1")
bool contiene(std::string nombre, std::string palabra)
{
	std::transform(nombre.begin(), nombre.end(), nombre.begin(), ::tolower); //todo a minusculas
	std::transform(palabra.begin(), palabra.end(), palabra.begin(), ::tolower);
	return nombre.find(palabra) != std::string::npos;
}

// Carga un archivo .obj (el que exporta Maya) y lo deja listo para dibujar, un VAO por cada grupo
// Cada vertice queda guardado como: x, y, z, nx, ny, nz (posicion + normal), por eso el stride es de 6 floats
bool cargarOBJ(const char* ruta, std::vector<Parte>& partes)
{
	std::ifstream archivo(ruta);
	if (!archivo.is_open()) return false;

	std::vector<glm::vec3> posiciones; //todos los vertices del archivo (lineas "v x y z")
	std::vector<std::vector<float>> datos; //x,y,z,nx,ny,nz de cada triangulo, separado por grupo
	std::vector<std::string> nombres;
	datos.push_back(std::vector<float>());
	nombres.push_back("sin_nombre");

	std::string linea;
	while (std::getline(archivo, linea))
	{
		std::istringstream ss(linea);
		std::string tipo;
		ss >> tipo;
		if (tipo == "v") //un vertice
		{
			float x, y, z;
			ss >> x >> y >> z;
			posiciones.push_back(glm::vec3(x, y, z));
		}
		else if (tipo == "o" || tipo == "g") //empieza un objeto o grupo nuevo
		{
			std::string nombre;
			std::getline(ss, nombre); //leemos TODO el resto de la linea (Maya escribe varias palabras, ej: "g helicoptero heliceGrande pCube11")
			if (!datos.back().empty()) { datos.push_back(std::vector<float>()); nombres.push_back(nombre); }
			else nombres.back() = nombre;
		}
		else if (tipo == "f") //una cara, formato "f v/vt/vn v/vt/vn ..." (solo usamos el numero de vertice)
		{
			std::vector<int> idx;
			std::string tok;
			while (ss >> tok)
			{
				int i = std::stoi(tok.substr(0, tok.find('/')));
				if (i < 0) i = (int)posiciones.size() + i + 1; //indices negativos cuentan desde el final
				idx.push_back(i - 1);
			}
			for (size_t k = 1; k + 1 < idx.size(); k++) //si la cara tiene mas de 3 lados la partimos en triangulos
			{
				int tri[3] = { idx[0], idx[k], idx[k + 1] };
				if (tri[0] < 0 || tri[0] >= (int)posiciones.size() ||
					tri[1] < 0 || tri[1] >= (int)posiciones.size() ||
					tri[2] < 0 || tri[2] >= (int)posiciones.size()) continue; //si algun indice es invalido saltamos este triangulo

				glm::vec3 p[3] = { posiciones[tri[0]], posiciones[tri[1]], posiciones[tri[2]] };

				// VECTOR NORMAL: el producto vectorial de dos lados del triangulo da un vector perpendicular a la cara (el "truco" de las diapositivas)
				glm::vec3 perpendicular = glm::cross(p[1] - p[0], p[2] - p[0]);
				glm::vec3 normal = (glm::length(perpendicular) > 0.0f) ? glm::normalize(perpendicular) : glm::vec3(0.0f, 1.0f, 0.0f); //normalizamos para que mida 1 (si el triangulo es una linea, usamos "arriba")

				for (int j = 0; j < 3; j++) //los 3 vertices del triangulo comparten la misma normal
				{
					datos.back().push_back(p[j].x); datos.back().push_back(p[j].y); datos.back().push_back(p[j].z);
					datos.back().push_back(normal.x); datos.back().push_back(normal.y); datos.back().push_back(normal.z);
				}
			}
		}
	}
	if (posiciones.empty()) return false;

	// centramos todo el modelo en el origen y lo escalamos para que mida 1 unidad
	glm::vec3 minimo(1e9f), maximo(-1e9f);
	for (size_t k = 0; k < datos.size(); k++)
		for (size_t i = 0; i + 5 < datos[k].size(); i += 6) //saltamos de 6 en 6 porque cada vertice tiene 6 numeros
		{
			glm::vec3 v(datos[k][i], datos[k][i + 1], datos[k][i + 2]);
			minimo = glm::min(minimo, v);
			maximo = glm::max(maximo, v);
		}
	glm::vec3 centro = (minimo + maximo) * 0.5f;
	glm::vec3 tamano = maximo - minimo;
	float mayor = std::fmax(tamano.x, std::fmax(tamano.y, tamano.z));

	for (size_t k = 0; k < datos.size(); k++)
	{
		if (datos[k].empty()) continue;
		for (size_t i = 0; i + 5 < datos[k].size(); i += 6)
		{
			datos[k][i] = (datos[k][i] - centro.x) / mayor; //solo se mueve y escala la posicion, las normales se dejan igual
			datos[k][i + 1] = (datos[k][i + 1] - centro.y) / mayor;
			datos[k][i + 2] = (datos[k][i + 2] - centro.z) / mayor;
		}

		Parte parte;
		parte.nombre = nombres[k];
		parte.cantidad = (int)(datos[k].size() / 6);

		// buscamos el centro de ESTA parte (punto medio entre su vertice mas chico y el mas grande)
		glm::vec3 minParte(1e9f), maxParte(-1e9f);
		for (size_t i = 0; i + 5 < datos[k].size(); i += 6)
		{
			glm::vec3 v(datos[k][i], datos[k][i + 1], datos[k][i + 2]);
			minParte = glm::min(minParte, v);
			maxParte = glm::max(maxParte, v);
		}
		parte.minimo = minParte;
		parte.maximo = maxParte;

		// subimos la parte a la GPU: VAO + VBO con el formato (x,y,z,nx,ny,nz)
		glGenVertexArrays(1, &parte.VAO); //Recordar siempre generar primero el VAO y luego el VBO
		glGenBuffers(1, &parte.VBO);
		glBindVertexArray(parte.VAO);
		glBindBuffer(GL_ARRAY_BUFFER, parte.VBO);
		glBufferData(GL_ARRAY_BUFFER, datos[k].size() * sizeof(float), datos[k].data(), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0); //posicion: 3 floats, empieza en el byte 0
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float))); //normal: 3 floats, empieza despues de los 3 de posicion
		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);

		std::cout << "Parte: " << parte.nombre << " (" << parte.cantidad << " vertices)" << std::endl;
		partes.push_back(parte);
	}
	return !partes.empty();
}


int main()
{
	// Inicializamos GLWW
	glfwInit();

	// Le decimos a GLFW que version de OPENGL vamos a usar
	//Para este caso el 3.3
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	// perfil de OPENGL que queremos usar , en este caso es el perfil de nucleo
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(800, 800, "Prueba 1", NULL, NULL);
	//Comprobacion de errores en el caso de que la ventana no se cree
	if (window == NULL)
	{
		std::cout << "Failed to create GFLW Window" << std::endl;

		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	gladLoadGL();
	glViewport(0, 0, 800, 800);
	glEnable(GL_DEPTH_TEST); //para que lo que esta mas cerca tape a lo que esta mas lejos

	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
	glCompileShader(vertexShader);

	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
	glCompileShader(fragmentShader);

	GLuint shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);

	// Si algun shader tiene un error de escritura, aqui nos lo dice en la consola (muy util cuando se toca la luz)
	GLint compilo;
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &compilo);
	if (!compilo)
	{
		char registro[1024];
		glGetProgramInfoLog(shaderProgram, 1024, NULL, registro);
		std::cout << "Error en los shaders: " << registro << std::endl;
	}

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	// ---- CARGAMOS EL HELICOPTERO ----
	std::vector<Parte> partes;
	if (!cargarOBJ("helicoptero.obj", partes)) //el archivo debe estar en la carpeta del proyecto (junto a Main.cpp)
	{
		std::cout << "No se pudo cargar helicoptero.obj" << std::endl;
		glfwTerminate();
		return -1;
	}
	std::cout << "Modelo cargado: " << partes.size() << " partes" << std::endl;

	// avisamos en la consola cuales partes se detectaron como helices (si no sale ninguna, el nombre en Maya es distinto)
	for (size_t k = 0; k < partes.size(); k++)
		if (contiene(partes[k].nombre, "heliceGrande") || contiene(partes[k].nombre, "heliceTraz"))
			std::cout << "Helice detectada: " << partes[k].nombre << std::endl;

	// CENTRO DE CADA HELICE: cada helice esta hecha de varias piezas, asi que el centro se saca con TODAS sus piezas juntas
	// (si cada pieza girara sobre su propio centro, las aspas se separarian)
	glm::vec3 minGrande(1e9f), maxGrande(-1e9f), minTraz(1e9f), maxTraz(-1e9f);
	for (size_t k = 0; k < partes.size(); k++)
	{
		if (contiene(partes[k].nombre, "heliceGrande"))
		{
			minGrande = glm::min(minGrande, partes[k].minimo);
			maxGrande = glm::max(maxGrande, partes[k].maximo);
		}
		else if (contiene(partes[k].nombre, "heliceTraz"))
		{
			minTraz = glm::min(minTraz, partes[k].minimo);
			maxTraz = glm::max(maxTraz, partes[k].maximo);
		}
	}
	glm::vec3 centroGrande = (minGrande + maxGrande) * 0.5f; //punto sobre el que gira la helice de arriba
	glm::vec3 centroTraz = (minTraz + maxTraz) * 0.5f; //punto sobre el que gira la helice de la cola

	// ---- LAS CURVAS DE BEZIER (cada una con sus 4 puntos en el espacio 3D) ----
	glm::vec3 rutaHeli[4] = //camino del helicoptero
	{
		glm::vec3(-3.0f,  0.0f,  0.0f), //P0: inicio
		glm::vec3(-1.0f,  3.0f, -4.0f), //P1: punto de control
		glm::vec3(2.0f, -2.0f, -4.0f), //P2: punto de control
		glm::vec3(4.0f,  1.0f,  0.0f)  //P3: final
	};
	glm::vec3 rutaCamara[4] = //camino de la camara cuando la ponemos a recorrer su propia curva
	{
		glm::vec3(-6.0f, 2.0f,  6.0f),
		glm::vec3(-2.0f, 6.0f,  2.0f),
		glm::vec3(4.0f, -1.0f,  6.0f),
		glm::vec3(6.0f, 3.0f, -2.0f)
	};

	// dibujamos la curva del helicoptero como una linea amarilla para ver el camino (50 puntos sobre la curva)
	std::vector<float> datosCurva;
	for (int i = 0; i <= 50; i++)
	{
		glm::vec3 p = bezier(rutaHeli, i / 50.0f);
		datosCurva.push_back(p.x); datosCurva.push_back(p.y); datosCurva.push_back(p.z);
	}
	GLuint VAOcurva, VBOcurva;
	glGenVertexArrays(1, &VAOcurva);
	glGenBuffers(1, &VBOcurva);
	glBindVertexArray(VAOcurva);
	glBindBuffer(GL_ARRAY_BUFFER, VBOcurva);
	glBufferData(GL_ARRAY_BUFFER, datosCurva.size() * sizeof(float), datosCurva.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	// ---- LA LAMPARA: un cubito que nos muestra donde esta la luz ----
	float cubo[] = //36 vertices (6 caras x 2 triangulos x 3 vertices), solo posicion
	{
		-0.5f, -0.5f,  0.5f,   0.5f, -0.5f,  0.5f,   0.5f,  0.5f,  0.5f,   0.5f,  0.5f,  0.5f,  -0.5f,  0.5f,  0.5f,  -0.5f, -0.5f,  0.5f, //frente
		-0.5f, -0.5f, -0.5f,  -0.5f,  0.5f, -0.5f,   0.5f,  0.5f, -0.5f,   0.5f,  0.5f, -0.5f,   0.5f, -0.5f, -0.5f,  -0.5f, -0.5f, -0.5f, //atras
		-0.5f,  0.5f,  0.5f,  -0.5f,  0.5f, -0.5f,  -0.5f, -0.5f, -0.5f,  -0.5f, -0.5f, -0.5f,  -0.5f, -0.5f,  0.5f,  -0.5f,  0.5f,  0.5f, //izquierda
		 0.5f,  0.5f,  0.5f,   0.5f, -0.5f, -0.5f,   0.5f,  0.5f, -0.5f,   0.5f, -0.5f, -0.5f,   0.5f,  0.5f,  0.5f,   0.5f, -0.5f,  0.5f, //derecha
		-0.5f,  0.5f, -0.5f,  -0.5f,  0.5f,  0.5f,   0.5f,  0.5f,  0.5f,   0.5f,  0.5f,  0.5f,   0.5f,  0.5f, -0.5f,  -0.5f,  0.5f, -0.5f, //arriba
		-0.5f, -0.5f, -0.5f,   0.5f, -0.5f, -0.5f,   0.5f, -0.5f,  0.5f,   0.5f, -0.5f,  0.5f,  -0.5f, -0.5f,  0.5f,  -0.5f, -0.5f, -0.5f  //abajo
	};
	GLuint VAOlampara, VBOlampara;
	glGenVertexArrays(1, &VAOlampara);
	glGenBuffers(1, &VBOlampara);
	glBindVertexArray(VAOlampara);
	glBindBuffer(GL_ARRAY_BUFFER, VBOlampara);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cubo), cubo, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	// ---- LA LUZ ----
	glm::vec3 lightPos = glm::vec3(0.0f, 5.0f, 4.0f); //donde esta la luz en el mundo (arriba y al frente del helicoptero)
	glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 1.0f); //luz blanca
	glm::vec3 colorHeli = glm::vec3(0.40f, 0.50f, 0.35f); //color del helicoptero (verde militar)

	// ---- CAMARA ----
	glm::vec3 cameraPos = glm::vec3(0.0f, 2.0f, 8.0f);
	glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
	glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
	float yaw = -90.0f;
	float pitch = 0.0f;
	bool camaraEnCurva = false; //false = camara libre con WASD, true = camara recorriendo su curva de Bezier mirando al helicoptero
	bool teclaCAnterior = false; //para detectar un solo "click" de la tecla C

	float deltaTime = 0.0f;
	float lastFrame = (float)glfwGetTime();

	while (!glfwWindowShouldClose(window))
	{
		glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUseProgram(shaderProgram);

		float currentFrame = (float)glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		// ---- TECLADO ----
		float velocidad = 2.5f * deltaTime;
		float velocidadGiro = 60.0f * deltaTime;

		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			glfwSetWindowShouldClose(window, true);

		// C: cambia entre camara libre y camara en la curva
		bool teclaC = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;
		if (teclaC && !teclaCAnterior)
			camaraEnCurva = !camaraEnCurva;
		teclaCAnterior = teclaC;

		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
			cameraPos += velocidad * cameraFront;
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
			cameraPos -= velocidad * cameraFront;
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
			cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * velocidad;
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
			cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * velocidad;
		if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
			cameraPos += velocidad * cameraUp;
		if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
			cameraPos -= velocidad * cameraUp;

		if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
			yaw -= velocidadGiro;
		if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
			yaw += velocidadGiro;
		if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
			pitch += velocidadGiro;
		if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
			pitch -= velocidadGiro;

		if (pitch > 89.0f) pitch = 89.0f;
		if (pitch < -89.0f) pitch = -89.0f;

		glm::vec3 front;
		front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
		front.y = sin(glm::radians(pitch));
		front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
		cameraFront = glm::normalize(front);

		// ---- POSICION DEL HELICOPTERO SOBRE SU CURVA ----
		// u va de 0 a 2 repetidamente: de 0 a 1 el helicoptero va de P0 a P3, de 1 a 2 regresa (ida y vuelta). 12 segundos por trayecto
		float u = fmod(currentFrame / 12.0f, 2.0f);
		float t = (u < 1.0f) ? u : 2.0f - u; //t es el parametro de la curva, siempre entre 0 y 1
		float sentido = (u < 1.0f) ? 1.0f : -1.0f; //al regresar la direccion se invierte
		glm::vec3 posHeli = bezier(rutaHeli, t);
		glm::vec3 direccion = bezierTangente(rutaHeli, t) * sentido; //hacia donde avanza en este instante
		float giroHeli = atan2(direccion.x, direccion.z); //angulo (eje Y) para que el helicoptero mire hacia donde avanza. Si tu modelo mira hacia otro lado, suma un offset (ej: + glm::radians(90.0f))

		// ---- VIEW: camara libre o camara en la curva ----
		glm::vec3 posCamaraActual; //donde esta la camara en este momento (la luz especular necesita saberlo)
		glm::mat4 view;
		if (camaraEnCurva)
		{
			float uc = fmod(currentFrame / 20.0f, 2.0f);
			float tc = (uc < 1.0f) ? uc : 2.0f - uc;
			posCamaraActual = bezier(rutaCamara, tc);
			view = glm::lookAt(posCamaraActual, posHeli, cameraUp); //la camara se mueve por su curva y siempre mira al helicoptero
		}
		else
		{
			posCamaraActual = cameraPos;
			view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
		}
		glm::mat4 projection = glm::perspective(glm::radians(45.0f), 800.0f / 800.0f, 0.1f, 100.0f);

		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, &view[0][0]);
		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, &projection[0][0]);

		// le mandamos al shader los datos de la luz y de la camara
		glUniform3f(glGetUniformLocation(shaderProgram, "lightPos"), lightPos.x, lightPos.y, lightPos.z);
		glUniform3f(glGetUniformLocation(shaderProgram, "lightColor"), lightColor.x, lightColor.y, lightColor.z);
		glUniform3f(glGetUniformLocation(shaderProgram, "viewPos"), posCamaraActual.x, posCamaraActual.y, posCamaraActual.z);

		// ---- DIBUJAMOS LA LINEA DE LA CURVA (sin luz) ----
		glm::mat4 modelCurva = glm::mat4(1.0f);
		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, &modelCurva[0][0]);
		glUniform1f(glGetUniformLocation(shaderProgram, "usarLuz"), 0.0f);
		glUniform3f(glGetUniformLocation(shaderProgram, "colorBase"), 1.0f, 1.0f, 0.0f); //amarillo
		glBindVertexArray(VAOcurva);
		glDrawArrays(GL_LINE_STRIP, 0, (int)(datosCurva.size() / 3));

		// ---- DIBUJAMOS LA LAMPARA (sin luz, siempre del color de la luz) ----
		glm::mat4 modelLampara = glm::mat4(1.0f);
		modelLampara = glm::translate(modelLampara, lightPos); //la ponemos donde esta la luz
		modelLampara = glm::scale(modelLampara, glm::vec3(0.3f)); //cubito pequeno
		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, &modelLampara[0][0]);
		glUniform3f(glGetUniformLocation(shaderProgram, "colorBase"), lightColor.x, lightColor.y, lightColor.z);
		glBindVertexArray(VAOlampara);
		glDrawArrays(GL_TRIANGLES, 0, 36);

		// ---- DIBUJAMOS EL HELICOPTERO (con luz) ----
		float escala = 1.5f; //tamano final del helicoptero (el modelo ya viene normalizado a 1 unidad)
		glm::mat4 modelHeli = glm::mat4(1.0f);
		modelHeli = glm::translate(modelHeli, posHeli); //lo movemos al punto de la curva
		modelHeli = glm::rotate(modelHeli, giroHeli, glm::vec3(0.0f, 1.0f, 0.0f)); //lo giramos hacia donde avanza
		modelHeli = glm::scale(modelHeli, glm::vec3(escala));
		glUniform1f(glGetUniformLocation(shaderProgram, "usarLuz"), 1.0f);
		glUniform3f(glGetUniformLocation(shaderProgram, "colorBase"), colorHeli.x, colorHeli.y, colorHeli.z);

		float giroHelice = currentFrame * 15.0f; //angulo de las helices: crece con el tiempo, asi que van girando (en radianes)

		for (size_t k = 0; k < partes.size(); k++)
		{
			glm::mat4 modelParte = modelHeli; //cada parte parte del mismo model del helicoptero

			// si la parte es una helice, la giramos sobre el centro de su helice: ir al centro -> girar -> volver
			if (contiene(partes[k].nombre, "heliceGrande")) //la helice de arriba gira alrededor del eje Y
			{
				modelParte = glm::translate(modelParte, centroGrande);
				modelParte = glm::rotate(modelParte, giroHelice, glm::vec3(0.0f, 1.0f, 0.0f));
				modelParte = glm::translate(modelParte, -centroGrande);
			}
			else if (contiene(partes[k].nombre, "heliceTraz")) //la helice de la cola gira alrededor del eje X
			{
				modelParte = glm::translate(modelParte, centroTraz);
				modelParte = glm::rotate(modelParte, giroHelice, glm::vec3(1.0f, 0.0f, 0.0f));
				modelParte = glm::translate(modelParte, -centroTraz);
			}

			glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, &modelParte[0][0]);

			// MATRIZ DE NORMALES: la transpuesta de la inversa de la parte 3x3 del model (asi las normales siguen perpendiculares aunque el objeto se mueva, rote o escale)
			glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(modelParte)));
			glUniformMatrix3fv(glGetUniformLocation(shaderProgram, "normalMatrix"), 1, GL_FALSE, &normalMatrix[0][0]);

			glBindVertexArray(partes[k].VAO);
			glDrawArrays(GL_TRIANGLES, 0, partes[k].cantidad);
		}

		glfwSwapBuffers(window);
		//Aqui se le dice a glfw que procese los eventos de la ventana SIN ESTO LA VENTANA NO RESPONDERA
		glfwPollEvents();
	}

	for (size_t k = 0; k < partes.size(); k++)
	{
		glDeleteVertexArrays(1, &partes[k].VAO);
		glDeleteBuffers(1, &partes[k].VBO);
	}
	glDeleteVertexArrays(1, &VAOcurva);
	glDeleteBuffers(1, &VBOcurva);
	glDeleteVertexArrays(1, &VAOlampara);
	glDeleteBuffers(1, &VBOlampara);
	glDeleteProgram(shaderProgram);

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}