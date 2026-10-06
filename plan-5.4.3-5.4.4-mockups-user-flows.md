# Plan para 5.4.3 y 5.4.4 — HydroGuard

Este documento define únicamente las vistas y flujos necesarios para documentar las aplicaciones web del Administrador y móvil del Operario. Los mock-ups y diagramas visuales todavía no se modifican.

## 5.4.3. Applications Mock-ups

### Aplicación web del Administrador

Mock-ups esenciales:

1. Inicio de sesión.
2. Registro conjunto de empresa y único Administrador.
3. Resumen de la organización.
4. Lista, creación y detalle de Operarios.
5. Grupos, reservorios y asignaciones.
6. Registro de dispositivo.
7. Resultado del registro con credencial técnica visible una sola vez.
8. Detalle del dispositivo con disponibilidad, asignación, configuración y estado de identidad (`Pendiente`, `Activa` o `Revocada`).
9. Confirmación de revocación de identidad.
10. Telemetría, procesos, alertas, incidentes, historial y reportes en modo de supervisión.

La identidad técnica se integra en Dispositivos; no requiere una sección principal ni una aplicación separada. Nunca se debe mostrar nuevamente la credencial en listas o detalles.

### Aplicación móvil del Operario

Mock-ups esenciales:

1. Primer acceso con código recibido por un medio externo, sin cambio obligatorio de contraseña.
2. Inicio de sesión.
3. Inicio y selección de reservorio asignado.
4. Detalle del reservorio y última medición.
5. Configuración operativa.
6. Proceso en medición y evaluación.
7. Estrategia propuesta en `PENDIENTE_APROBACION_CORRECCION` con acción `Aprobar corrección`.
8. Corrección, espera y reevaluación automática sin nuevas aprobaciones.
9. Resultado `LISTO`, `FALLO` o `EMERGENCIA`.
10. Confirmación de liberación manual disponible únicamente en `LISTO`.
11. Alertas recibidas mediante FCM, historial y perfil.

### Criterios comunes

- Aplicar el Design System vigente del frontend web y su adaptación móvil.
- No depender únicamente del color para comunicar estados.
- Mostrar dispositivo, reservorio, proceso y última actualización en acciones críticas.
- Incluir carga, vacío, error, sin conexión y permiso denegado cuando sean relevantes.
- Diferenciar el producto integral, la simulación y el prototipo académico sin cambiar las reglas del dominio.

## 5.4.4. Applications User Flow Diagrams

Se requiere un User Flow por objetivo, no por cada pantalla.

### Flujos web

#### UF-W01 — Registrar empresa y Administrador

**User goal:** crear la organización y su única cuenta administradora.

- Happy path: ingresar datos → validar → crear organización y Administrador → iniciar sesión.
- Unhappy paths: organización existente, correo duplicado o datos inválidos.

#### UF-W02 — Incorporar un Operario

**User goal:** dejar al Operario listo para ingresar y administrar sus asignaciones.

- Happy path: crear cuenta con contraseña permanente → completar perfil → asignar grupo → asignar reservorio-dispositivo → generar código → enviarlo externamente.
- Unhappy paths: perfil incompleto, dispositivo no registrado, recurso ya asignado o cuenta duplicada.

#### UF-W03 — Registrar y provisionar un dispositivo

**User goal:** registrar un dispositivo y obtener su credencial técnica.

- Happy path: ingresar inventario, entorno y capacidades → backend registra y provisiona identidad → mostrar credencial una sola vez → confirmar resguardo → ver detalle.
- Unhappy paths: serie duplicada, capacidad incompatible o fallo de provisionamiento. El reintento no debe duplicar el inventario.

#### UF-W04 — Revocar la identidad de un dispositivo

**User goal:** impedir que un dispositivo comprometido continúe comunicándose.

- Happy path: abrir detalle → elegir revocar → confirmar → mostrar estado `Revocada`.
- Unhappy paths: ya revocada, sin permiso o error de servicio.

### Flujos móviles

#### UF-M01 — Primer acceso del Operario

**User goal:** ingresar a la cuenta preparada por el Administrador.

- Happy path: introducir código → validar → iniciar sesión directamente → abrir inicio.
- Unhappy paths: código inválido, utilizado o revocado. El código no caduca y no obliga a cambiar la contraseña.

#### UF-M02 — Configurar un reservorio-dispositivo

**User goal:** publicar una configuración operativa compatible.

- Happy path: seleccionar asignación → completar rangos, estrategia, dosis, espera, ciclos y modo de liberación → revisar → publicar.
- Unhappy paths: datos inválidos, capacidad incompatible, versión desactualizada o pérdida de conexión.

#### UF-M03 — Aprobar y supervisar un tratamiento

**User goal:** autorizar la estrategia una vez y seguir el tratamiento hasta su resultado.

- Happy path: iniciar proceso → medir → detectar no conformidad → revisar estrategia propuesta → aprobar una vez → ejecutar corrección → esperar → reevaluar → repetir ciclos automáticamente → alcanzar `LISTO`.
- Unhappy paths: no aprobar, perder autorización sobre el dispositivo, actuación rechazada, límite alcanzado, pérdida de monitoreo o emergencia.
- Regla: mientras no se apruebe, no se ordena ninguna actuación y la válvula permanece cerrada.

#### UF-M04 — Liberar agua en modo manual

**User goal:** confirmar el momento de liberación de agua ya conforme.

- Happy path: proceso `LISTO` y modo manual → confirmar → abrir válvula → finalizar.
- Unhappy paths: proceso no listo, autorización vencida, emergencia o fallo del actuador. El Operario nunca puede reemplazar la conformidad del sistema.

#### UF-M05 — Atender una notificación

**User goal:** abrir desde FCM el proceso o alerta que requiere atención.

- Happy path: recibir notificación → abrir → validar sesión y asignación → mostrar detalle relacionado.
- Unhappy paths: token FCM inválido, sesión vencida, alerta ya resuelta o asignación retirada. La alerta interna permanece disponible aunque falle FCM.

## Entregables pendientes

- [ ] Mock-ups actualizados en Figma.
- [ ] User Flows diagramados en FigJam, Lucidchart u Overflow.
- [ ] Texto explicativo breve debajo de cada mock-up y cada User Flow.
- [ ] Verificación de consistencia con los Wireflows.
- [ ] Revisión de accesibilidad, estados alternativos y fidelidad con las aplicaciones implementadas.
