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

	glClearColor(0.07f,0.13f,0.17f,1.0f); /*aqui le indicamos a OPENGL que se prepare para borrar uun color de un buffer y darle otro color
	(se le da color de la forma decimal normalizada del color RGB(0,0,0 y alfa(que se la tranparencia del color 1 es opaco 0 es transparente))
	*/

	glClear(GL_COLOR_BUFFER_BIT);//ahora queremos que OPENGL ejecute el comando para el que le hemos indicado que se prepare usaremos esta funcion
	glfwSwapBuffers(window); //Aqui se intercambia el orden en el que se ven los buffers [hasta el momento se veia el color del buffer frontal] 


	/* Para evitar que se cierre la ventana necesitamos crear un bucle,
	con la condicion que lav entana se cierre la condicion se dara cuando pulsemos el boton para que se cierre
	*/
	while (!glfwWindowShouldClose(window)); 
	{
		glfwPollEvents(); //Aqui se le dice a glfw que procese los eventos de la ventana SIN ESTO LA VENTANA NO RESPONDERA
	}
	                                        

	//Funcion para eliminar la ventana, pero hace que apenas se abra se cierre 
	glfwDestroyWindow(window); 
	//Termina GLFW despues de finalizar el programa
	glfwTerminate();
	return 0;
}