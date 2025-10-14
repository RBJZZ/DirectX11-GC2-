# Knight's Oath - Motor Gráfico en DirectX 11

Este repositorio contiene el proyecto final para la materia de **Gráficas Computacionales II**. El proyecto es un motor gráfico 3D construido desde cero utilizando **DirectX 11**, con una escena de juego de aventura en tercera persona como demostración.

- **Universidad:** Universidad Autónoma de Nuevo León (UANL)
- **Facultad:** Facultad de Ciencias Físico Matemáticas (FCFM)
- **Curso:** Gráficas Computacionales II (Grupo 053)
- **Equipo:**
    - 1972507 Rebeca Evangelista Jasso
    - 1961794 David Aguilar Acosta
    - 2004626 Brandon Zarif Mendoza Avendaño

---

## ⚙️ Características Técnicas

* **API Gráfica:** DirectX 11
* **Arquitectura:** Motor 3D modular con pipeline de Forward Rendering multi-pasada.
* **Iluminación:** Modelo de iluminación **Phong** (ambiental, difusa, especular) por píxel.
* **Sombras Dinámicas:** Implementación de **Shadow Mapping** en tiempo real. Soporta **Alpha Clipping** para sombras correctas en texturas con transparencias (e.g., follaje de árboles).
* **Ciclo de Día y Noche:** Sistema dinámico que modifica en tiempo real la dirección, color e intensidad de la luz direccional y ambiental.
* **Post-Procesado:** Cadena de efectos de post-procesado, incluyendo un efecto de **Bloom** (HDR) con desenfoque Gaussiano.
* **Terreno por Heightmap:** Generación de terreno a partir de un mapa de alturas con texturizado multi-capa basado en altura y pendiente (`slope`).
* **Carga de Modelos:** Integración de la librería **Assimp** para la carga de mallas y materiales desde archivos `.obj`.
* **Sistema de Partículas:** Renderizado de partículas con **billboarding** y blending aditivo para efectos como luciérnagas.
* **UI y Elementos 2D:** Renderizado de texturas 2D para la interfaz, incluyendo un **minimapa** funcional que se actualiza en tiempo real.
* **Cámara y Colisiones:** Cámara en tercera persona con detección de colisiones AABB (Axis-Aligned Bounding Box) contra la geometría del escenario.

---

## 🏛️ Arquitectura del Motor

### Núcleo y Clases Principales
* **`Game.h/.cpp`**: Orquestador principal. Gestiona el bucle del juego, el ciclo de vida de los objetos y la lógica de alto nivel.
* **`DeviceResources.h/.cpp`**: Capa de abstracción de hardware (HAL) que encapsula la inicialización y gestión del dispositivo D3D11, la swap chain y los contextos.
* **`RenderTexture.h/.cpp`**: Componente clave que permite renderizar escenas a texturas, sirviendo como base para todos los efectos de renderizado multi-pasada.

### Pipeline de Renderizado
El motor utiliza un pipeline de **Forward Rendering** con las siguientes pasadas secuenciales:

1.  **Pase de Sombras (Shadow Pass)**:
    * **Entrada:** Geometría de la escena.
    * **Proceso:** Se renderiza la escena desde el punto de vista de la luz direccional.
    * **Salida:** Un mapa de profundidad (`ID3D11ShaderResourceView`) que se utiliza en el pase principal.

2.  **Pase Principal (Main Pass)**:
    * **Entrada:** Geometría de la escena, mapa de sombras.
    * **Proceso:** Se renderiza la escena desde la perspectiva de la cámara a una textura fuera de pantalla (`off-screen texture`). Los pixel shaders calculan la iluminación Phong y muestrean el mapa de sombras para aplicar las sombras.
    * **Salida:** Una textura HDR con la escena completamente iluminada.

3.  **Pase de Post-Procesado (Post-FX Pass)**:
    * **Entrada:** Textura HDR del pase principal.
    * **Proceso:** Se ejecuta una cadena de shaders sobre un quad que cubre la pantalla. Incluye extracción de brillos, desenfoque Gaussiano (horizontal y vertical en pasadas separadas) y composición final para el efecto de Bloom.
    * **Salida:** Una textura LDR con los efectos aplicados.

4.  **Pase de UI (UI Pass)**:
    * **Entrada:** Textura LDR del pase de Post-FX.
    * **Proceso:** Se renderiza la textura final al back buffer. Encima, se dibujan elementos 2D como el minimapa y texto de depuración.
    * **Salida:** Frame final presentado en pantalla.

---

## 🎮 Controles

* **Movimiento:** `W`, `A`, `S`, `D`
* **Correr:** Doble toque en `W`
* **Cámara:** Mover el `Mouse`
* **Salir:** `Esc`

---

## 🚀 Compilación y Ejecución

1.  Clonar el repositorio.
2.  Abrir `GC2_PlantillaDB.sln` con **Visual Studio 2022**.
3.  Asegurar que el **SDK de Windows 10/11** esté instalado.
4.  Establecer la configuración en `Debug` y la plataforma en `x64`.
5.  Compilar y ejecutar (**F5**).
