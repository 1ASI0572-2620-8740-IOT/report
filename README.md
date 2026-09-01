<p align="center">
    <img src="./assets/logo-upc.png" alt="upc-logo" width="80px" height="80px"/>
</p>

<h1 align="center">
    Universidad Peruana de Ciencias Aplicadas
</h1>

<h3 align="center">
    Carrera: Ingeniería de Software
    <br> <br>
    Curso: 1ASI0572 - Desarrollo de Soluciones IoT
    <br> <br>
    Sección: 8740
    <br> <br>
    Profesor: David Carlos Vera Olivera
    <br> <br>
    Ciclo: 2026-20
    <br> <br>
    Informe de Trabajo Final
    <br> <br>
    Startup: NOMBRE_DE_STARTUP_POR_DEFINIR
    <br> <br>
    Producto: NOMBRE_DE_PRODUCTO_POR_DEFINIR
</h3>

<div align="center">

| <div style="width:300px">Alumno</div> | <div style="width:125px">Código</div> |
|:-------------------------------------------:|:-------------------------------------------:|
|       Gomez Hurtado, Miguel Angel     |              u202220294                     |
|         Rodriguez Macedo, Sebastian       |                  u202310199                 |
|       Santur Tello, Andrea Elizabeth        |               u202310988                    |
|  Prieto Mantari, Leonardo Fabrizzio Junior  |              u202319949                     |
|         Rios Pacheco, Hector Javier         |              u20231c540                     |
|         Olivera Barzola, Eric Marlon         |            u202315032                       |

</div>

<div align="center"> Septiembre 2026 </div>



## Registro de Versiones del Informe

| **Versión** | **Fecha**     | **Autor(es)**                                                                                   | **Descripción de modificación**                                                                                                                                             |
|-------------|---------------|--------------------------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| |  | | |


## Project Report Collaboration Insights

Las actividades del proyecto se planificarán, asignarán y evidenciarán progresivamente en la organización del equipo en GitHub: <https://github.com/1ASI0572-2620-8740-IOT>.


## Tabla de Contenidos

- [Student Outcome](#student-outcome)
- [Capítulo I: Introducción](#capítulo-i-introducción)
  - [1.1. Startup Profile](#11-startup-profile)
    - [1.1.1. Descripción de la startup](#111-descripción-de-la-startup)
    - [1.1.2. Perfiles de integrantes del equipo](#112-perfiles-de-integrantes-del-equipo)
  - [1.2. Solution Profile](#12-solution-profile)
    - [1.2.1. Antecedentes y problemática](#121-antecedentes-y-problemática)
    - [1.2.2. Lean UX Process](#122-lean-ux-process)
      - [1.2.2.1. Lean UX Problem Statements](#1221-lean-ux-problem-statements)
      - [1.2.2.2. Lean UX Assumptions](#1222-lean-ux-assumptions)
      - [1.2.2.3. Lean UX Hypothesis Statements](#1223-lean-ux-hypothesis-statements)
      - [1.2.2.4. Lean UX Canvas](#1224-lean-ux-canvas)
  - [1.3. Segmentos objetivo](#13-segmentos-objetivo)
    - [Segmento 1: micro y pequeñas empresas textiles](#segmento-1-micro-y-pequeñas-empresas-textiles-con-teñido-o-acabado)
    - [Segmento 2: pequeños productores y microempresas hidropónicas](#segmento-2-pequeños-productores-y-microempresas-hidropónicas)
- [Capítulo II: Requirements Elicitation & Analysis](#capítulo-ii-requirements-elicitation--analysis)
  - [2.1. Competidores](#21-competidores)
    - [2.1.1. Análisis competitivo](#211-análisis-competitivo)
    - [2.1.2. Estrategias y tácticas frente a competidores](#212-estrategias-y-tácticas-frente-a-competidores)
  - [2.2. Entrevistas](#22-entrevistas)
    - [2.2.1. Diseño de entrevistas](#221-diseño-de-entrevistas)
    - [2.2.2. Registro de entrevistas](#222-registro-de-entrevistas)
    - [2.2.3. Análisis de entrevistas](#223-análisis-de-entrevistas)
  - [2.3. Needfinding](#23-needfinding)
    - [2.3.1. User Personas](#231-user-personas)
    - [2.3.2. User Task Matrix](#232-user-task-matrix)
    - [2.3.3. User Journey Mapping](#233-user-journey-mapping)
    - [2.3.4. Empathy Mapping](#234-empathy-mapping)
  - [2.4. Big Picture EventStorming](#24-big-picture-eventstorming)
  - [2.5. Ubiquitous Language](#25-ubiquitous-language)
- [Capítulo III: Requirements Specification](#capítulo-iii-requirements-specification)
  - [3.1. User Stories](#31-user-stories)
  - [3.2. Impact Mapping](#32-impact-mapping)
  - [3.3. Product Backlog](#33-product-backlog)
- [Capítulo IV: Solution Software Design](#capítulo-iv-solution-software-design)
  - [4.1. Strategic-Level Domain-Driven Design](#41-strategic-level-domain-driven-design)
    - [4.1.1. Design-Level EventStorming](#411-design-level-eventstorming)
      - [4.1.1.1. Candidate Context Discovery](#4111-candidate-context-discovery)
      - [4.1.1.2. Domain Message Flows Modeling](#4112-domain-message-flows-modeling)
      - [4.1.1.3. Bounded Context Canvases](#4113-bounded-context-canvases)
    - [4.1.2. Context Mapping](#412-context-mapping)
    - [4.1.3. Software Architecture](#413-software-architecture)
      - [4.1.3.1. Software Architecture System Landscape Diagram](#4131-software-architecture-system-landscape-diagram)
      - [4.1.3.2. Software Architecture Context Level Diagrams](#4132-software-architecture-context-level-diagrams)
      - [4.1.3.2. Software Architecture Container Level Diagrams](#4132-software-architecture-container-level-diagrams)
      - [4.1.3.3. Software Architecture Deployment Diagrams](#4133-software-architecture-deployment-diagrams)
  - [4.2. Tactical-Level Domain-Driven Design](#42-tactical-level-domain-driven-design)
    - [4.2.X. Bounded Context: NOMBRE_DE_BOUNDED_CONTEXT_POR_DEFINIR](#42x-bounded-context-nombre_de_bounded_context_por_definir)
      - [4.2.X.1. Domain Layer](#42x1-domain-layer)
      - [4.2.X.2. Interface Layer](#42x2-interface-layer)
      - [4.2.X.3. Application Layer](#42x3-application-layer)
      - [4.2.X.4. Infrastructure Layer](#42x4-infrastructure-layer)
      - [4.2.X.5. Bounded Context Software Architecture Component Level Diagrams](#42x5-bounded-context-software-architecture-component-level-diagrams)
      - [4.2.X.6. Bounded Context Software Architecture Code Level Diagrams](#42x6-bounded-context-software-architecture-code-level-diagrams)
        - [4.2.X.6.1. Bounded Context Domain Layer Class Diagrams](#42x61-bounded-context-domain-layer-class-diagrams)
        - [4.2.X.6.2. Bounded Context Database Design Diagram](#42x62-bounded-context-database-design-diagram)
- [Bibliografía](#bibliografía)
- [Anexos](#anexos)

## Student Outcome

**ABET – EAC - Student Outcome 5.** La capacidad de funcionar efectivamente en un equipo cuyos miembros juntos proporcionan liderazgo, crean un entorno de colaboración e inclusivo, establecen objetivos, planifican tareas y cumplen objetivos.

En el siguiente cuadro se describirán las acciones realizadas y las conclusiones del grupo que sustenten el logro del ABET – EAC - Student Outcome 5. Las celdas se completarán de manera acumulativa durante las entregas del proyecto.

| Criterios específicos | Acciones realizadas | Conclusiones |
|:--|:--|:--|
| Trabaja en equipo para proporcionar liderazgo en forma conjunta. |  |  |
| Crea un entorno colaborativo e inclusivo, establece metas, planifica tareas y cumple objetivos. |  |  |


