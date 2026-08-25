#include<iostream>
#include<glad/glad.h>
#include<GLFW/glfw3.h>
//configurar shaders de OpenGL
const char* vertexShaderSource = "#version 330 core\n"
"layout (location = 0) in vec3 aPos;\n"
"void main()\n"
"{\n"
"   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
"}\0";

const char* fragmentShaderSource = "#version 330 core\n"
"out vec4 FragColor;\n"
"void main()\n"
"{\n"
"   FragColor = vec4(0.8f, 0.3f, 0.02f, 1.0f);\n"
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
		-0.5f, -0.5f * float(sqrt(3)) / 3, 0.0f, // Primer vertice
		0.5f, -0.5f * float(sqrt(3)) / 3, 0.0f, // Segundo vertice
		0.0f, 0.5f * float(sqrt(3)) * 2 / 3, 0.0f  // Tercer vertice
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
	glViewport(0,0, 800, 800); //indicandole (la parte izquierda de nuestra ventana (0,0,hasta la esquina superior derecha 800,800)

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

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0); //Aqui le decimos a OPENGL como interpretar los datos que le estamos pasando
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);


	/*aqui le indicamos a OPENGL que se prepare para borrar uun color de un buffer y darle otro color
   (se le da color de la forma decimal normalizada del color RGB(0,0,0 y alfa(que se la tranparencia del color 1 es opaco 0 es transparente))
   */
	glClearColor(0.07f,0.13f,0.17f,1.0f);
	//ahora queremos que OPENGL ejecute el comando para el que le hemos indicado que se prepare usaremos esta funcion
	glClear(GL_COLOR_BUFFER_BIT);
	//Aqui se intercambia el orden en el que se ven los buffers [hasta el momento se veia el color del buffer frontal] 
	glfwSwapBuffers(window); 


	/* Para evitar que se cierre la ventana necesitamos crear un bucle,
	con la condicion que lav entana se cierre la condicion se dara cuando pulsemos el boton para que se cierre
	*/
	while (!glfwWindowShouldClose(window))
	{

		glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glUseProgram(shaderProgram);
		glBindVertexArray(VAO);
		glDrawArrays(GL_TRIANGLES, 0, 3);	
		glfwSwapBuffers(window);
		//Aqui se le dice a glfw que procese los eventos de la ventana SIN ESTO LA VENTANA NO RESPONDERA
		glfwPollEvents(); 


	}


	glDeleteVertexArrays(1, &VAO);	
	glDeleteBuffers(1, &VBO);
	glDeleteProgram(shaderProgram);

	//Funcion para eliminar la ventana, pero hace que apenas se abra se cierre 
	glfwDestroyWindow(window); 
	//Termina GLFW despues de finalizar el programa
	glfwTerminate();
	return 0;
}