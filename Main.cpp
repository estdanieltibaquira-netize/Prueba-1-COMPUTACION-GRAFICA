#include<iostream>
#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#define STB_IMAGE_IMPLEMENTATION //esto solo se pone UNA vez en todo el proyecto, aqui en el main
#include <stb_image.h>
//configurar shaders de OpenGL
const char* vertexShaderSource = "#version 330 core\n" //le decimos a OpenGL que usamos GLSL version 3.30, perfil core
"layout (location = 0) in vec3 aPos;\n" //aqui llega la posicion (x,y,z) de cada vertice, la misma que ya tenias
"layout (location = 1) in vec2 aTexCoord;\n" //coordenada de textura de este vertice
"out vec2 texCoord;\n" //se la pasamos al fragment shader
"uniform mat4 model;\n" //matriz de 4x4 que vamos a mandar desde el codigo C++, "uniform" quiere decir que es un valor que le pasamos nosotros y que es igual para los 3 vertices en esa pasada (no cambia vertice por vertice como aPos)
"uniform mat4 view;\n" //donde esta parada la camara y hacia donde mira
"uniform mat4 projection;\n" //le da la perspectiva (como el lente de una camara real, con profundidad)
"void main()\n"
"{\n"
"   gl_Position = projection * view * model * vec4(aPos.x, aPos.y, aPos.z, 1.0);\n" //el orden importa: primero se mueve/rota el objeto (model), luego se ubica segun la camara (view), luego se le da la perspectiva (projection)
"   texCoord = aTexCoord;\n" //la mandamos tal cual, sin transformar
"}\0";

const char* fragmentShaderSource = "#version 330 core\n" //version de GLSL, igual que en el vertex shader
"out vec4 FragColor;\n" //color final que va a salir pintado en pantalla
"in vec2 texCoord;\n" //la coordenada que llego del vertex shader (ya interpolada)
"uniform sampler2D tex0;\n" //la textura en si
"uniform float alpha;\n" //transparencia que le vamos a mandar desde C++ para cada triangulo
"void main()\n"
"{\n"
"   vec4 texColor = texture(tex0, texCoord);\n" //sacamos el color de la imagen en ese pixel
"   FragColor = vec4(texColor.rgb, texColor.a * alpha);\n" //el color viene de la imagen, el alpha se combina con el que ya tenias
"}\n\0";


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

	GLfloat vertices[] =
	{
		// posiciones                                    // coordenadas de textura (u, v)
		-0.5f, -0.5f * float(sqrt(3)) / 3, 0.0f,          0.0f, 0.0f, // Primer vertice (abajo izq.)
		0.5f, -0.5f * float(sqrt(3)) / 3, 0.0f,           1.0f, 0.0f, // Segundo vertice (abajo der.)
		0.0f, 0.5f * float(sqrt(3)) * 2 / 3, 0.0f,        0.5f, 1.0f  // Tercer vertice (arriba)
	};


	//para la ventana tomamaos datos de un tipo de ventana en GLFW y utilizamos una funcion para crear una ventana toma 5 entrandas (ancho, alto, "nombre", NULL, NULL)
	GLFWwindow* window = glfwCreateWindow(800, 800, "Prueba 1", NULL, NULL);
	//Comprobacion de errores en el caso de que la ventana no se cree
	if (window == NULL)
	{
		std::cout << "Failed to create GFLW Window" << std::endl;

		glfwTerminate();
		return -1;
	}
	//GLFW no entiende que si creamos una ventana es para usarla por eso se le indica que debe usarse
	glfwMakeContextCurrent(window);

	//Para darle color a la ventana le pedimos a glad que cargue las configuraiones necesarias para OPENGL

	gladLoadGL();

	// Indicamos el area de la ventana la cual queremos que OPENGL se renderice 
	glViewport(0, 0, 800, 800); //indicandole (la parte izquierda de nuestra ventana (0,0,hasta la esquina superior derecha 800,800)

	glEnable(GL_BLEND); //le decimos a OPENGL que active la mezcla de colores para las cosas transparentes
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); //la formula de esa mezcla: usa el alpha del color nuevo para decidir cuanto se ve el nuevo color y cuanto el que ya estaba pintado
	glEnable(GL_DEPTH_TEST); //le dice a OPENGL que tenga en cuenta que hay cosas mas cerca o mas lejos de la camara, y las pinte en el orden correcto

	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER); //Creamos un shader de tipo VERTEX SHADER y lo guardamos en una variable de tipo GLuint que es un entero posito en OPENGL
	glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
	/*
	la GPU NO entiende el codigo fuente y toca compilarlo en codigo binario que la GPU si entiende,
	para eso se usa esta funcion que toma 4 parametros(el shader que queremos compilar,
	el numero de lineas de codigo fuente, el codigo fuente y NULL)
	*/
	glCompileShader(vertexShader);


	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
	glCompileShader(fragmentShader);

	//para poder usar los shaders que hemos creado necesitamos encapsular en algo llamado programa shader
	GLuint shaderProgram = glCreateProgram(); //creamos un programa shader y lo guardamos
	// y para adjuntar un shader a un programa shader usamos la funcion glAttachShader

	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);

	//eliminamos los sombreadores que teniamos antes de crear el programa shader ya que no los necesitamos
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);


	GLuint VAO, VBO;

	glGenVertexArrays(1, &VAO); //Recoadar siempre generar primero el VAO y luego el VBO, ya que el VAO es el que guarda la configuracion del VBO
	glGenBuffers(1, &VBO);

	glBindVertexArray(VAO); //Enlazamos VAO para poder trabajar con el

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0); //posicion: 3 floats, empieza en el byte 0
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float))); //textura: 2 floats, empieza despues de los 3 de posicion
	glEnableVertexAttribArray(1);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	GLuint texture;
	glGenTextures(1, &texture); //creamos un "espacio" para la textura, como hicimos con VAO/VBO
	glBindTexture(GL_TEXTURE_2D, texture); //la enlazamos para trabajar con ella

	// como se repite/estira la textura si el triangulo pide mas area de la que hay imagen
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	// como se ve la textura cuando esta muy cerca (MAG) o muy lejos (MIN) de la camara
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	stbi_set_flip_vertically_on_load(true); //la imagen normalmente viene "al reves" para OPENGL, esto la voltea bien

	int widthImg, heightImg, numColCh;
	unsigned char* bytes = stbi_load("textura.jpg", &widthImg, &heightImg, &numColCh, 0); //carga la imagen del disco a memoria (cambia el nombre por el de tu imagen)

	if (bytes)
	{
		GLenum formato = (numColCh == 4) ? GL_RGBA : GL_RGB; //si la imagen tiene canal alfa (png) usa RGBA, si no (jpg) usa RGB
		glTexImage2D(GL_TEXTURE_2D, 0, formato, widthImg, heightImg, 0, formato, GL_UNSIGNED_BYTE, bytes); //le pasamos los pixeles a OPENGL
		glGenerateMipmap(GL_TEXTURE_2D); //genera versiones mas pequeñas automaticamente, para cuando se vea de lejos
	}
	else
	{
		std::cout << "No se pudo cargar la textura" << std::endl;
	}

	stbi_image_free(bytes); //ya se la pasamos a la GPU, no necesitamos esta copia en RAM
	glBindTexture(GL_TEXTURE_2D, 0);


	/*aqui le indicamos a OPENGL que se prepare para borrar uun color de un buffer y darle otro color
   (se le da color de la forma decimal normalizada del color RGB(0,0,0 y alfa(que se la tranparencia del color 1 es opaco 0 es transparente))
   */
	glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
	//ahora queremos que OPENGL ejecute el comando para el que le hemos indicado que se prepare usaremos esta funcion
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); //ahora tambien limpiamos el buffer de profundidad en cada frame
	//Aqui se intercambia el orden en el que se ven los buffers [hasta el momento se veia el color del buffer frontal] 
	glfwSwapBuffers(window);

	glm::vec3 posiciones[3] = //aqui guardamos el punto (centro/pivote) donde va a quedar y girar cada uno de los 3 triangulos
	{
		glm::vec3(-0.3f,  0.2f, 0.0f), //triangulo 1: arriba a la izquierda
		glm::vec3(0.3f,  0.2f, 0.0f), //triangulo 2: arriba a la derecha
		glm::vec3(0.0f, -0.3f, 0.0f)  //triangulo 3: abajo al centro
	};

	// (el arreglo de colores ya no se usa: el color de cada triangulo ahora sale de la textura, no de un color fijo)

	float alphas[3] = { 0.7f, 0.4f, 1.0f }; //transparencia de cada triangulo: 0.7 = 30% transparente, 0.4 = 60% transparente, 1.0 = nada transparente


	/* Para evitar que se cierre la ventana necesitamos crear un bucle,
	con la condicion que lav entana se cierre la condicion se dara cuando pulsemos el boton para que se cierre
	*/
	while (!glfwWindowShouldClose(window))
	{

		glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); //ahora tambien limpiamos el buffer de profundidad en cada frame
		glUseProgram(shaderProgram);
		glBindVertexArray(VAO);
		float angulo = (float)glfwGetTime() * 90.0f; //segundos desde que arranco el programa, multiplicado por 50 para que gire mas rapido

		float camAngulo = (float)glfwGetTime() * 8.0f; //grados que ha girado la camara (mas lento que los triangulos, para que se note bien)
		float radio = 4.0f; //que tan lejos esta la camara del centro donde estan los triangulos

		glm::vec3 camPos = glm::vec3(radio * sin(glm::radians(camAngulo)), 2.0f, radio * cos(glm::radians(camAngulo))); //posicion de la camara: se mueve en circulo alrededor del centro (0,0,0)
		glm::mat4 view = glm::lookAt(camPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)); //la camara siempre mira hacia el centro, con "arriba" en Y

		glm::mat4 projection = glm::perspective(glm::radians(45.0f), 800.0f / 800.0f, 0.1f, 100.0f); //el "lente" de la camara: 45 grados de apertura, relacion ancho/alto de la ventana, y hasta donde alcanza a ver

		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, &view[0][0]); //mandamos la camara al shader
		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, &projection[0][0]); //mandamos la perspectiva al shader

		glActiveTexture(GL_TEXTURE0); //activamos la "ranura" de textura 0
		glBindTexture(GL_TEXTURE_2D, texture); //enlazamos nuestra textura ahi
		glUniform1i(glGetUniformLocation(shaderProgram, "tex0"), 0); //le decimos al shader que "tex0" lea de esa ranura 0

		for (int i = 0; i < 3; i++) //se repite 3 veces, una por cada triangulo
		{
			glm::mat4 model = glm::mat4(1.0f); //matriz identidad, punto de partida sin ninguna transformacion
			model = glm::translate(model, posiciones[i]); //la movemos al pivote de este triangulo
			model = glm::rotate(model, glm::radians(angulo), glm::vec3(0.0f, 1.0f, 0.0f)); //la rotamos sobre el eje X

			glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, &model[0][0]); //mandamos la matriz al shader
			glUniform1f(glGetUniformLocation(shaderProgram, "alpha"), alphas[i]); //mandamos la transparencia de este triangulo

			glDrawArrays(GL_TRIANGLES, 0, 3); //dibujamos el triangulo con esa matriz, color y alpha ya puestos
		}
		glfwSwapBuffers(window);
		//Aqui se le dice a glfw que procese los eventos de la ventana SIN ESTO LA VENTANA NO RESPONDERA
		glfwPollEvents();


	}


	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
	glDeleteTextures(1, &texture);
	glDeleteProgram(shaderProgram);

	//Funcion para eliminar la ventana, pero hace que apenas se abra se cierre 
	glfwDestroyWindow(window);
	//Termina GLFW despues de finalizar el programa
	glfwTerminate();
	return 0;
}