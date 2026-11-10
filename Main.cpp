#include<iostream>
#include<fstream>
#include<sstream>
#include<vector>
#include<string>
#include<cmath>
#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>

//configurar shaders de OpenGL
const char* vertexShaderSource = "#version 330 core\n"
"layout (location = 0) in vec3 aPos;\n" //posicion (x,y,z) de cada vertice
"out vec3 fragPos;\n" //posicion en el mundo, la usamos para la luz falsa
"uniform mat4 model;\n"
"uniform mat4 view;\n"
"uniform mat4 projection;\n"
"void main()\n"
"{\n"
"   vec4 mundo = model * vec4(aPos, 1.0);\n"
"   fragPos = mundo.xyz;\n"
"   gl_Position = projection * view * mundo;\n" //model -> view -> projection
"}\0";

const char* fragmentShaderSource = "#version 330 core\n"
"out vec4 FragColor;\n"
"in vec3 fragPos;\n"
"uniform vec3 colorBase;\n" //color del objeto
"uniform float usarLuz;\n" //1.0 = sombrear el helicoptero, 0.0 = color plano (para la linea de la curva)
"void main()\n"
"{\n"
"   vec3 c = colorBase;\n"
"   if (usarLuz > 0.5)\n"
"   {\n"
"      vec3 n = normalize(cross(dFdx(fragPos), dFdy(fragPos)));\n" //normal de cada cara calculada con derivadas
"      float d = abs(dot(n, normalize(vec3(0.5, 1.0, 0.7))));\n" //luz falsa fija, solo para que se note el volumen
"      c = colorBase * (0.35 + 0.65 * d);\n"
"   }\n"
"   FragColor = vec4(c, 1.0);\n"
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
};

// Carga un archivo .obj (el que exporta Maya) y lo deja listo para dibujar, un VAO por cada grupo
bool cargarOBJ(const char* ruta, std::vector<Parte>& partes)
{
	std::ifstream archivo(ruta);
	if (!archivo.is_open()) return false;

	std::vector<glm::vec3> posiciones; //todos los vertices del archivo (lineas "v x y z")
	std::vector<std::vector<float>> datos; //x,y,z de cada triangulo, separado por grupo
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
			ss >> nombre;
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
				for (int j = 0; j < 3; j++)
				{
					if (tri[j] < 0 || tri[j] >= (int)posiciones.size()) continue;
					glm::vec3 p = posiciones[tri[j]];
					datos.back().push_back(p.x); datos.back().push_back(p.y); datos.back().push_back(p.z);
				}
			}
		}
	}
	if (posiciones.empty()) return false;

	// centramos todo el modelo en el origen y lo escalamos para que mida 1 unidad
	glm::vec3 minimo(1e9f), maximo(-1e9f);
	for (size_t k = 0; k < datos.size(); k++)
		for (size_t i = 0; i + 2 < datos[k].size(); i += 3)
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
		for (size_t i = 0; i + 2 < datos[k].size(); i += 3)
		{
			datos[k][i] = (datos[k][i] - centro.x) / mayor;
			datos[k][i + 1] = (datos[k][i + 1] - centro.y) / mayor;
			datos[k][i + 2] = (datos[k][i + 2] - centro.z) / mayor;
		}

		Parte parte;
		parte.nombre = nombres[k];
		parte.cantidad = (int)(datos[k].size() / 3);

		// subimos la parte a la GPU: VAO + VBO con el formato (x,y,z), stride de 3 floats
		glGenVertexArrays(1, &parte.VAO); //Recordar siempre generar primero el VAO y luego el VBO
		glGenBuffers(1, &parte.VBO);
		glBindVertexArray(parte.VAO);
		glBindBuffer(GL_ARRAY_BUFFER, parte.VBO);
		glBufferData(GL_ARRAY_BUFFER, datos[k].size() * sizeof(float), datos[k].data(), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
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
		glm::mat4 view;
		if (camaraEnCurva)
		{
			float uc = fmod(currentFrame / 20.0f, 2.0f);
			float tc = (uc < 1.0f) ? uc : 2.0f - uc;
			glm::vec3 posCamCurva = bezier(rutaCamara, tc);
			view = glm::lookAt(posCamCurva, posHeli, cameraUp); //la camara se mueve por su curva y siempre mira al helicoptero
		}
		else
		{
			view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
		}
		glm::mat4 projection = glm::perspective(glm::radians(45.0f), 800.0f / 800.0f, 0.1f, 100.0f);

		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, &view[0][0]);
		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, &projection[0][0]);

		// ---- DIBUJAMOS LA LINEA DE LA CURVA ----
		glm::mat4 modelCurva = glm::mat4(1.0f);
		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, &modelCurva[0][0]);
		glUniform1f(glGetUniformLocation(shaderProgram, "usarLuz"), 0.0f);
		glUniform3f(glGetUniformLocation(shaderProgram, "colorBase"), 1.0f, 1.0f, 0.0f); //amarillo
		glBindVertexArray(VAOcurva);
		glDrawArrays(GL_LINE_STRIP, 0, (int)(datosCurva.size() / 3));

		// ---- DIBUJAMOS EL HELICOPTERO ----
		float escala = 1.5f; //tamano final del helicoptero (el modelo ya viene normalizado a 1 unidad)
		glm::mat4 modelHeli = glm::mat4(1.0f);
		modelHeli = glm::translate(modelHeli, posHeli); //lo movemos al punto de la curva
		modelHeli = glm::rotate(modelHeli, giroHeli, glm::vec3(0.0f, 1.0f, 0.0f)); //lo giramos hacia donde avanza
		modelHeli = glm::scale(modelHeli, glm::vec3(escala));
		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, &modelHeli[0][0]);
		glUniform1f(glGetUniformLocation(shaderProgram, "usarLuz"), 1.0f);
		glUniform3f(glGetUniformLocation(shaderProgram, "colorBase"), 0.75f, 0.75f, 0.75f); //gris
		for (size_t k = 0; k < partes.size(); k++)
		{
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
	glDeleteProgram(shaderProgram);

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}