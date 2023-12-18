# GPURayTracer
An OpenGL PathTracer with Phong Shaded free camera movement.
Code is still a bit messed up.

Includes:
- Efficient ray-triangle intersection with only one division, if its a non-occluded hit
- Fresnel Effect
- Area Light Sampling with
    - $A \cdot I$ weighted Monte Carlo for diffuse Lighting<br>
      Example: Area $A = 4$, Intensity $I = 1$ is equivalent to $A = 1$, $I = 4$, $A\cdot I$ is the weighting of this area light
    - Occlusion test for specular reflections
- PBR Texture support (OpenEXR format) with Tangent Space Shading and Oren-Nayar diffuse shading model.
- Russian roulette canceling of current path
- OpenGL forward rendering for comparison or complex scene movement
- sRGB / DCI-P3 ToneMapping
- OpenEXR 32 bit linear float export