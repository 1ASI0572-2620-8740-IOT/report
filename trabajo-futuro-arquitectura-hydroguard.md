# Trabajo futuro de arquitectura de HydroGuard

Este archivo separa las actividades que todavía no corresponden a la corrección de lo actualmente desarrollado. No forman parte del alcance ejecutado en esta actualización.

## Diagramas e imágenes

- Actualizar Big Picture EventStorming con el provisionamiento y la autenticación del dispositivo.
- Corregir Candidate Context Discovery para ubicar espera, reevaluación y liberación dentro de Treatment.
- Actualizar Design-Level EventStorming con la selección automática y aprobación única de la estrategia.
- Separar visualmente los flujos de fallo y emergencia.
- Añadir el flujo de autenticación HTTPS/REST a Domain Message Flows.
- Crear el canvas de `Device Identity and Access`.
- Actualizar el Context Map con el sexto bounded context.
- Diferenciar System Landscape y System Context, cuyas imágenes actuales están duplicadas.
- Actualizar Container y Deployment Diagram con web para el Administrador, móvil para el Operario, Edge API, Device Identity y FCM, sin MQTT.
- Validar los diagramas con el profesor antes de sustituir las imágenes actuales.

## Backend y Edge

- Implementar `Device Identity and Access` con provisionamiento, hashing, autenticación y revocación.
- Implementar los endpoints HTTPS/REST definidos para Edge.
- Emitir tokens de corta duración con dispositivo, organización, audiencia y scopes.
- Implementar polling de comandos y acknowledgements idempotentes.
- Implementar la aprobación única y continuación automática en Treatment.
- Implementar el adaptador FCM en Monitoring.

## Aplicación móvil Flutter

- Implementar con BLoC la estrategia propuesta y su aprobación única.
- Mostrar los ciclos posteriores como continuación automática.
- Mantener la liberación manual disponible únicamente desde `READY`.
- Registrar el token FCM y navegar desde una notificación hacia el proceso o alerta correspondiente.

## Firmware y simulación

- Autenticar ESP32/Wokwi mediante HTTPS.
- Enviar telemetría y heartbeat autenticados.
- Consultar comandos y confirmar su resultado de forma idempotente.
- Mantener el LED como representación de la actuación en el prototipo académico.
- Modificar el sensor en la simulación para representar el efecto de la corrección.
