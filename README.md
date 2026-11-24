# Helicóptero iluminado en OpenGL

**Autor:** Daniel Tibaquira
**Materia:** Computación Gráfica · Ingeniería Multimedia · Universidad Militar Nueva Granada
**Docente:** Fernando J. Nossa 
**Video explicativo:** \[https://youtu.be/h7GJDNDCFN4]

## Descripción

Escena 3D hecha con OpenGL 3.3 (C++). Un helicóptero cargado desde un archivo OBJ recorre una curva de Bézier cúbica en el espacio, con sus dos hélices girando. Está iluminado con el modelo de Phong (luz ambiental, difusa y especular) y se puede ver con una cámara libre o con una cámara que también viaja por su propia curva de Bézier.

## Tecnologías

|Herramienta|Para qué se usa|
|-|-|
|C++ y Visual Studio 2026|Lenguaje y entorno de desarrollo|
|OpenGL 3.3 core|Dibujo en la GPU|
|GLFW|Ventana y entradas de teclado|
|GLAD|Carga de las funciones de OpenGL|
|GLM|Matemáticas: vectores y matrices|
|Maya|Exportación del modelo del helicóptero a OBJ|

## Controles (entradas de teclado)

|Tecla|Acción|
|-|-|
|W / S|Mover la cámara hacia adelante / atrás|
|A / D|Mover la cámara a la izquierda / derecha|
|Espacio / Shift izquierdo|Subir / bajar la cámara|
|Flechas|Girar la vista|
|C|Cambiar entre cámara libre y cámara en curva de Bézier|
|Esc|Cerrar el programa|

## Arquitectura del software

```mermaid
flowchart TD
    A(\[Inicio]) --> B\["1. Ventana<br/>GLFW + GLAD"]
    B --> C\["2. Shaders<br/>compilar y enlazar"]
    C --> D\["3. Cargar modelo<br/>OBJ a VAO y VBO"]
    D --> E\["4. Curvas y luz<br/>Bézier, lámpara, lightPos"]
    E --> F{"Bucle principal<br/>cada fotograma"}
    F --> G\["A. Entrada<br/>teclado y cámara"]
    G --> H\["B. Movimiento<br/>posición en la curva"]
    H --> I\["C. Uniforms<br/>luz y cámara al shader"]
    I --> J\["D. Dibujar<br/>partes y hélices girando"]
    J --> K\["GPU<br/>vertex shader y fragment shader"]
    K --> L\["Pantalla<br/>swap de buffers"]
    L --> F
    F -->|se cierra la ventana| M\["5. Limpiar y cerrar"]
    M --> N(\[Fin])
```

### Qué hace cada bloque

|Bloque|Qué hace|Dónde está en `Main.cpp`|
|-|-|-|
|1. Ventana|Inicia GLFW, pide OpenGL 3.3 core, crea la ventana de 800×800, carga GLAD y activa el depth test|`main`: `glfwInit`, `glfwCreateWindow`, `gladLoadGL`|
|2. Shaders|Compila el vertex shader y el fragment shader y los une en un programa|`vertexShaderSource`, `fragmentShaderSource`, `glLinkProgram`|
|3. Cargar modelo|Lee el OBJ, calcula la normal de cada triángulo y sube cada parte a la GPU con su VAO y VBO|`cargarOBJ`|
|4. Curvas y luz|Define los puntos de las curvas de Bézier, la lámpara y la posición y color de la luz|`rutaHeli`, `rutaCamara`, `lightPos`, `lightColor`|
|A. Entrada|Lee el teclado y mueve o gira la cámara|bucle `while`, sección TECLADO|
|B. Movimiento|Calcula el punto de la curva donde va el helicóptero y hacia dónde mira|`bezier`, `bezierTangente`|
|C. Uniforms|Envía al shader la luz, la cámara y las matrices|`glUniform...`|
|D. Dibujar|Dibuja la curva, la lámpara y el helicóptero parte por parte; las hélices giran|sección DIBUJAMOS|
|5. Limpiar y cerrar|Borra los buffers y cierra GLFW|final de `main`|

## Conceptos aplicados

**Matrices.** Cada objeto usa tres matrices: `model` (traslación, rotación y escala), `view` (la cámara, con `glm::lookAt`) y `projection` (perspectiva, con `glm::perspective`).

**Traslación de la cámara.** La cámara libre se mueve sumando su dirección a su posición con las teclas WASD. En modo curva, su posición sale de una curva de Bézier y siempre mira al helicóptero.

**Rotación de objetos.** El helicóptero gira sobre el eje Y para mirar hacia donde avanza. Las hélices giran con `glm::rotate` sobre su propio centro: la hélice grande sobre el eje Y y la de la cola sobre el eje X.

**Curva de Bézier cúbica.** Se define con cuatro puntos:

`B(t) = (1-t)³·P0 + 3(1-t)²·t·P1 + 3(1-t)·t²·P2 + t³·P3`, con `t` entre 0 y 1.

Su derivada indica la dirección de avance en cada punto y se usa para orientar el helicóptero.

**Iluminación de Phong.** Se calcula en el fragment shader, para cada píxel:

* **Ambiental:** una luz baja y constante para que nada quede negro.
* **Difusa:** `max(dot(normal, direcciónLuz), 0)`; la cara brilla más mientras más de frente le llega la luz.
* **Especular:** el punto brillante del reflejo, que depende de la posición de la cámara.

La normal de cada triángulo se calcula con el producto vectorial y se transforma con la matriz de normales.

**Carga del modelo.** El helicóptero se cargó desde un archivo OBJ exportado de Maya, leído por la función `cargarOBJ`.

## Estructura del repositorio

|Archivo|Contenido|
|-|-|
|`Main.cpp`|Todo el programa|
|`helicoptero.obj`|Modelo 3D del helicóptero|
|`glad.c`|Cargador de OpenGL|
|`libraries/`|Archivos de GLFW|
|`Prueba 1.slnx` y `Prueba 1.vcxproj`|Proyecto de Visual Studio|



