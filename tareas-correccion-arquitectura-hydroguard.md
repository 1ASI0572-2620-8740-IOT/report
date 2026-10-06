# Correcciones aplicadas a la arquitectura de HydroGuard

Este documento registra únicamente las correcciones realizadas sobre la documentación y el frontend que ya existían. No contiene tareas de implementación futura.

## README y definición del dominio

- Se incorporó `Device Identity and Access` como bounded context separado de Human IAM, Configuration y Telemetry.
- Se precisó que este contexto administra la identidad técnica, credencial, autenticación, estado y revocación de los dispositivos.
- Se mantuvo en Configuration el inventario del dispositivo, su entorno, capacidades, reservorio y asignación al Operario.
- Se aclaró que Edge API y Firebase Cloud Messaging son componentes de integración, no bounded contexts.
- Se corrigió la distribución de aplicaciones: Administrador en la aplicación web y Operario en la aplicación móvil.
- Se corrigió US-33 para que la consulta móvil corresponda al Operario y solo incluya sus dispositivos asignados.

## Tratamiento y liberación

- Treatment selecciona automáticamente la estrategia correctiva.
- El Operario aprueba la estrategia una sola vez antes del primer ciclo.
- Antes de la aprobación, el proceso permanece en `PENDIENTE_APROBACION_CORRECCION`, no se emiten actuaciones y la válvula continúa cerrada.
- Después de la aprobación, los ciclos posteriores continúan automáticamente hasta alcanzar `LISTO` o `FALLO`.
- Se corrigió US-15 para representar la aprobación única y la ejecución posterior.
- Se actualizó US-31 para mostrar la estrategia propuesta, el estado pendiente y el progreso automático.
- Se corrigió el diseño táctico de Treatment con `CorrectionApproval`, el comando `ApproveCorrectionCommand` y su endpoint.
- Se eliminó la regla incorrecta que permitía liberar agua por la sola confirmación del Operario.
- La liberación manual y automática solo puede ejecutarse cuando el backend informa el estado `LISTO`.
- Se mantuvieron fallo y emergencia como situaciones diferentes.

## Identidad técnica y comunicación de dispositivos

- Se amplió TS-01 con provisionamiento, credencial propia, vínculo entre `deviceId` y `organizationId`, autenticación y revocación.
- Se definió que la credencial original se muestra una sola vez y que el backend conserva únicamente su hash.
- Se definieron los estados `PENDING`, `ACTIVE` y `REVOKED`.
- Se corrigió TS-04 para utilizar HTTPS/REST y una identidad obtenida del token autenticado.
- Se eliminaron MQTT y RabbitMQ de la especificación vigente.
- Se documentaron los contratos de autenticación, telemetría, heartbeat, consulta de comandos y acknowledgements.
- Se corrigió Telemetry para que no administre credenciales ni confíe en el `deviceId` enviado libremente en el payload.
- Se retiró turbidez del modelo táctico y de los DTO de Telemetry porque no pertenece al alcance vigente.

## Human IAM y Configuration

- Se corrigió Human IAM para representar una sola cuenta administradora activa por organización.
- Se eliminó el registro público de Operarios y la asignación libre de roles.
- Se documentó que el Administrador crea la cuenta del Operario con contraseña permanente.
- Se mantuvo el primer acceso mediante código sin caducidad y sin cambio obligatorio de contraseña.
- Se actualizó el modelo relacional con `organizationId`, un único rol por cuenta y una restricción para el Administrador.
- Se precisaron grupos, reservorios, perfiles de Operario y asignaciones.
- Se corrigió el registro de dispositivos para coordinar desde backend el alta de inventario con el provisionamiento de identidad.

## Monitoring y notificaciones

- Se precisó el uso de Firebase Cloud Messaging como servicio externo de notificaciones móviles.
- Se añadió un puerto de notificaciones y el adaptador `FirebaseCloudMessagingAdapter` en el diseño táctico.
- Se estableció que una falla de FCM no elimina, resuelve ni modifica la alerta persistida por Monitoring.
- Se corrigió TS-11 con recepción, fallo, reintento y navegación desde la notificación.

## Frontend y mock API existentes

- Se añadió `identityStatus` al modelo de dispositivo.
- El registro devuelve el dispositivo y una credencial de activación visible una sola vez.
- La interfaz permite copiar la credencial y continuar al detalle.
- El detalle muestra el estado de identidad técnica.
- Se añadió la acción de revocación con confirmación.
- Se evitó crear un menú o módulo frontend independiente para Device Identity.
- El mock mantiene las identidades en `deviceIdentities` y no persiste la credencial original.
- Se alinearon DTO, mapper, repositorio Axios, caso de uso, vistas y datos mock.
- Se corrigió la etiqueta genérica `ACCIÓN IRREVERSIBLE` por `CONFIRMACIÓN REQUERIDA`.

## Documentos complementarios

- Se actualizó `plan-5.4.3-5.4.4-mockups-user-flows.md` con los flujos acordados.
- Se actualizó `.internal-docs/guia_front.md` con seis bounded contexts, aprobación única, HTTPS/REST y FCM.
- Se añadieron referencias oficiales de Firebase Cloud Messaging al README.

## Verificación realizada

- Compilación Angular completada correctamente mediante `npm run build`.
- Verificación contractual del mock completada con 88 comprobaciones HTTP aprobadas.
- Revisión visual completada para registro, presentación única de credencial, detalle y revocación.
- Se confirmó que ningún archivo de imagen o diagrama fue modificado.
