# Guía de desarrollo de aplicaciones frontend — HydroGuard

## 1. Propósito

Esta guía organiza el desarrollo frontend de HydroGuard en dos aplicaciones independientes según el rol y el contexto de uso:

- **Aplicación móvil para el Operario:** desarrollada con Flutter y Dart, utilizando el patrón BLoC. Está orientada a la intervención directa sobre el dispositivo y el proceso de tratamiento.
- **Aplicación web para el Administrador:** desarrollada con Angular y TypeScript, organizada con DDD por bounded context. Está orientada a la administración, configuración base y supervisión de su organización.

Las aplicaciones consumen los mismos contratos del backend, pero no ofrecen las mismas funciones. La separación evita que el Operario reciba herramientas administrativas innecesarias y que el Administrador ejecute accidentalmente acciones operativas sobre un dispositivo.

## 2. Modelo operativo y flujo completo de incorporación

HydroGuard es una solución multiempresa. Cada organización se registra con una única cuenta administradora, y pueden coexistir varias organizaciones sin compartir usuarios ni recursos. El Administrador de cada empresa prepara la estructura operativa desde la aplicación web antes de entregar acceso a un Operario. El Operario recibe externamente un código de primer acceso, entra directamente en la aplicación móvil y completa la configuración operativa de los reservorios y dispositivos que ya le fueron asignados.

### Estructura y cardinalidades

```text
Organización
├── un Administrador
└── uno o varios Grupos de trabajo
    ├── uno o varios Perfiles de Operario
    │   └── un Operario por perfil
    └── uno o varios Reservorios
        └── un Dispositivo activo por reservorio
            └── cero o un Operario responsable
```

| Relación | Regla acordada |
|:--|:--|
| Organización → Administrador | Cada organización tiene exactamente un Administrador en el alcance actual. |
| Organización → grupos | La organización puede tener uno o varios grupos. |
| Grupo → Operarios | Un grupo puede tener uno o varios Operarios. |
| Operario → grupo | Cada Operario pertenece a un solo grupo. |
| Grupo → reservorios | Un grupo tiene uno o varios reservorios. |
| Reservorio → dispositivo | Cada reservorio tiene exactamente un dispositivo activo. |
| Dispositivo → reservorio | Cada dispositivo está vinculado a un solo reservorio. |
| Operario → dispositivos | Un Operario puede manejar uno o varios dispositivos dentro de su grupo. |
| Dispositivo → Operario | Un dispositivo tiene como máximo un Operario responsable; nunca se comparte. |

El término **reservorio** representa el contenedor controlado. Según el segmento, la interfaz podrá mostrar “fosa”, “tanque” o “reservorio”, pero el contrato conservará un identificador general `reservoirId` y un tipo de reservorio.

Cada organización selecciona un único segmento durante su registro. Los grupos, reservorios, dispositivos y perfiles operativos heredan ese segmento y no pueden redefinirlo. Un grupo puede registrar un propósito, cultivo, proceso o área para separar funciones dentro de la empresa, pero esa clasificación no crea un segmento adicional.

### Diferencia entre perfil y configuración

- **Perfil de Operario:** registro administrativo creado antes del primer acceso. Contiene la cuenta, el grupo y las asignaciones de reservorio-dispositivo.
- **Configuración operativa:** reglas que el Operario completa desde Flutter para cada par reservorio-dispositivo: rangos, estrategia, dosis o intensidad, espera, límite de ciclos y modo de liberación.

El Perfil de Operario puede contener varias asignaciones cuando la misma persona gestiona más de un reservorio. Cada asignación conserva su propia configuración versionada para evitar que un cambio en un reservorio altere otro proceso.

### Registro público de la organización y su Administrador

1. El futuro Administrador abre la ruta pública de registro de la aplicación web.
2. Registra el nombre o razón social, RUC, teléfono y único segmento de la empresa.
3. Registra su nombre completo, correo electrónico y contraseña definitiva.
4. El backend valida que el RUC y el correo no estén registrados.
5. El backend crea la organización y su única cuenta administradora dentro de una misma transacción.
6. El Administrador inicia sesión con su correo y contraseña.

El registro público se utiliza exclusivamente para incorporar una organización junto con su Administrador. Los Operarios no se autorregistran y continúan siendo creados por el Administrador de su empresa.

### Flujo del Administrador en la aplicación web

1. **Crear el grupo de trabajo.** Registra nombre, propósito y, para hidroponía, el tipo de planta o cultivo cuando corresponda. El grupo hereda el segmento de la organización.
2. **Registrar el reservorio.** Define nombre, código interno, tipo, ubicación y grupo. La capacidad es opcional mientras no se utilice para calcular dosis.
3. **Registrar y activar el dispositivo.** Registra número de serie, alias, modelo, entorno y capacidades. El alta lo deja inmediatamente en `ACTIVE_UNLINKED`; la disponibilidad `ONLINE` u `OFFLINE` se controla por separado.
4. **Vincular dispositivo y reservorio.** El sistema rechaza la operación si alguno ya posee una vinculación activa. El dispositivo pasa a `ACTIVE_UNASSIGNED`.
5. **Comprobar la unidad operativa.** El Administrador verifica que el reservorio y su dispositivo estén activos antes de crear el perfil.
6. **Crear la cuenta del Operario.** El Administrador define nombre, identificador de acceso y la contraseña definitiva. El Operario no tendrá que cambiarla durante este alcance.
7. **Crear el Perfil de Operario.** Selecciona el único grupo al que pertenecerá la persona y agrega uno o varios pares reservorio-dispositivo disponibles de ese grupo.
8. **Generar el código de primer acceso.** El sistema crea un código único asociado a la cuenta y al perfil completo.
9. **Entregar el acceso externamente.** El sistema no envía invitaciones durante este alcance; el Administrador comunica el código, el identificador y la contraseña definitiva por el medio que considere apropiado. El código permite el primer ingreso directo y las credenciales se utilizan en accesos posteriores.
10. **Supervisar la activación.** El perfil permanece `PENDING_FIRST_ACCESS` hasta que el Operario use el código.

El Administrador no puede generar el código si falta la cuenta, el grupo, al menos un reservorio o su dispositivo vinculado. Tampoco puede asignar al perfil un dispositivo que ya tenga otro responsable.

### Primer acceso del Operario en Flutter

1. El Operario abre “Ingresar con código”.
2. Introduce el código recibido del Administrador.
3. El backend valida que exista, no haya sido utilizado ni revocado y que el perfil continúe completo.
4. La aplicación muestra la organización, grupo y los reservorios-dispositivos asignados para que el Operario compruebe el contexto.
5. Al confirmar, el código inicia directamente la primera sesión y pasa a `USED`.
6. El perfil pasa a `ACTIVE` y la app muestra todos sus reservorios; no se solicita cambio de contraseña.
7. El Operario selecciona cada reservorio y completa su configuración operativa.
8. Hasta publicar una configuración válida, puede consultar el dispositivo, pero no iniciar un proceso de tratamiento.

Para simplificar el prototipo, el código no vence por tiempo. Sigue siendo de un solo uso y el Administrador puede revocarlo o generar otro mientras no haya sido utilizado. Los accesos posteriores se realizan con el identificador y la contraseña definitiva creada por el Administrador.

### Desvinculación y reasignación

- Desvincular un reservorio-dispositivo cierra la asignación vigente y lo deja `UNASSIGNED`.
- No existe transferencia automática entre Operarios.
- Para reasignarlo, el Administrador abre el perfil del nuevo Operario y agrega manualmente el par disponible.
- La asignación anterior permanece en el historial y no se sobrescribe.
- La ausencia, suplencia o delegación temporal queda fuera del alcance.
- Usuarios, grupos, reservorios, dispositivos y perfiles se desactivan mediante eliminación lógica; no se borran si tienen trazabilidad.

### Visibilidad dentro del grupo

El Operario puede ver los nombres de los demás integrantes de su grupo y qué reservorio tiene asignado cada uno. No puede consultar detalles operativos, configuración, mediciones ni ejecutar comandos sobre dispositivos ajenos.

### Separación por aplicación

| Aspecto | Aplicación móvil del Operario | Aplicación web del Administrador |
|:--|:--|:--|
| Usuario | Operario de un grupo con uno o más reservorios-dispositivos asignados. | Único Administrador de su organización, sin dispositivo propio. |
| Propósito | Completar configuraciones y operar directamente sus reservorios. | Preparar grupos, cuentas, reservorios, dispositivos, perfiles y supervisar el sistema. |
| Tecnología | Flutter y Dart. | Angular y TypeScript. |
| Patrón principal | BLoC por feature. | DDD frontend por bounded context. |
| Comunicación HTTP | Retrofit para Dart, con clientes tipados y Dio únicamente como transporte interno. | Axios con interceptores. |
| Alcance de datos | Sus asignaciones y la identificación básica de los integrantes de su grupo y organización. | Todos los usuarios, grupos, reservorios, dispositivos, procesos e incidentes de su organización. |
| Acciones críticas | Configurar, iniciar proceso, liberar manualmente, emergencia y restablecimiento. | Registrar, vincular, asignar, desvincular, generar código y supervisar. |
| Presentación | Uso rápido en campo, selector de reservorio y prioridad operativa. | Tablas, filtros, formularios administrativos y vistas comparativas. |

No se implementará una interfaz única que cambie completamente según el rol. Cada aplicación tendrá su propio repositorio o proyecto, navegación, despliegue y ciclo de entrega.

## 3. Alcance del sprint

### Incluido

- Estructura inicial de ambas aplicaciones.
- Funciones frontend relacionadas con los cinco bounded contexts.
- Separación estricta entre las capacidades del Operario y del Administrador.
- Material Design como lenguaje visual común.
- Angular Material para la aplicación web.
- Widgets Material de Flutter para la aplicación móvil.
- Integración desacoplada con API mediante Retrofit en Flutter y Axios en Angular.
- Backend mock inicial mediante un servidor Node.js y `db.json`, conservando los contratos previstos para el backend real.
- Estados de carga, contenido vacío, error, desconexión y falta de permisos.
- Propuesta inicial de pantallas y navegación, pendiente de observaciones antes de formalizar diseños en Figma.

### Excluido

- Landing page, porque ya fue desarrollada.
- Pruebas unitarias durante este sprint.
- Backend productivo, Edge API, firmware y simulación Wokwi.
- Implementación de reglas de dosificación, conformidad o autorización dentro de las aplicaciones.

### Estado actual frente a la hoja de ruta

Esta guía describe el destino funcional de ambas aplicaciones, no implica que todos los contextos estén implementados. En el estado vigente, la aplicación web dispone de la base de IAM multiempresa; la creación de grupos, reservorios, dispositivos y asignaciones aún corresponde a trabajo posterior. La aplicación móvil y las interfaces de los demás bounded contexts permanecen pendientes.

| Bounded context | Aplicación web | Aplicación móvil |
|:--|:--|:--|
| Identity and Access Management | Implementación inicial disponible. | Pendiente. |
| Device and Operational Configuration | Pendiente. | Pendiente. |
| IoT Telemetry and Device Integration | Pendiente. | Pendiente. |
| Water Quality Treatment and Release | Pendiente. | Pendiente. |
| Operational Monitoring and Traceability | Pendiente. | Pendiente. |
- Diseño visual definitivo en Figma.

## 4. Principios funcionales compartidos

1. El backend es la autoridad sobre estado, ciclos, dosificación, fallos y liberación.
2. Las aplicaciones muestran decisiones del dominio; no intentan reproducirlas localmente.
3. Cada acción remota debe indicar progreso, aceptación, rechazo o fallo.
4. Un comando aceptado no equivale a una actuación completada.
5. Las acciones críticas no admiten doble envío.
6. La válvula se presenta cerrada salvo cuando exista una liberación autorizada.
7. Una actuación confirmada no significa que el agua sea conforme; se debe esperar la reevaluación.
8. Los términos visibles deben coincidir con el Ubiquitous Language del README.
9. Las dos aplicaciones deben distinguir `PRODUCTO_INTEGRAL`, `SIMULACION` y `PROTOTIPO_ACADEMICO`.
10. La intervención manual se describe únicamente como sustitución demostrativa del prototipo académico.
11. Cada consulta y comando se limita a la organización obtenida de la sesión; el cliente no decide libremente el alcance mediante `organizationId`.

## 5. Contratos compartidos entre aplicaciones

Aunque Dart y TypeScript requieren modelos propios, ambas aplicaciones deben conservar los mismos nombres de campos, enumeraciones y significados.

### Identificadores mínimos

- `organizationId`
- `userId`
- `operatorId`
- `groupId`
- `reservoirId`
- `deviceId`
- `operatorProfileId`
- `assignmentId`
- `firstAccessCodeId`
- `configurationId`
- `configurationVersion`
- `processId`
- `cycleNumber`
- `measurementId`
- `commandId`
- `correlationId`
- `alertId`
- `incidentId`
- `reportId`

### Enumeraciones compartidas

```text
UserRole:
  OPERATOR | ADMINISTRATOR

OrganizationSegment:
  TEXTILE | HYDROPONIC

OperatingEnvironment:
  INTEGRAL_PRODUCT | SIMULATION | ACADEMIC_PROTOTYPE

ProcessState:
  NOT_STARTED | MEASURING | EVALUATING | CORRECTING | WAITING |
  REEVALUATING | READY | RELEASING | FAILED | EMERGENCY | COMPLETED

CommandStatus:
  PENDING | ACCEPTED | RUNNING | COMPLETED | REJECTED | FAILED

ReleaseMode:
  MANUAL | AUTOMATIC

DeviceAvailability:
  ONLINE | DELAYED | OFFLINE | UNKNOWN

DeviceLifecycleStatus:
  ACTIVE_UNLINKED | ACTIVE_UNASSIGNED | ACTIVE_ASSIGNED | MAINTENANCE | INACTIVE

OperatorProfileStatus:
  PENDING_FIRST_ACCESS | ACTIVE | INACTIVE

FirstAccessCodeStatus:
  ACTIVE | USED | REVOKED

AssignmentStatus:
  ACTIVE | CLOSED
```

Las traducciones se aplican únicamente en la capa de presentación. Los valores enviados a la API deben permanecer estables.

## 6. Backend mock con servidor Node.js y `db.json`

### Objetivo

El mock permite construir navegación, formularios, estados y consumo HTTP antes de disponer del backend real. La implementación vigente de la aplicación administrativa utiliza `mock-api/server.mjs` como servidor Node.js y `mock-api/db.json` como persistencia local. Este servidor reproduce las rutas, validaciones, comandos y restricciones que necesita el frontend, pero no sustituye la seguridad ni la lógica transaccional del backend productivo.

El mock se encuentra actualmente en el proyecto web. La aplicación Flutter podrá consumir sus contratos mientras ambos proyectos puedan acceder a la misma instancia HTTP, sin copiar ni modificar directamente el archivo `db.json`.

### Estructura recomendada

```text
mock-api/
├── db.json
└── server.mjs
```

### Colecciones mínimas de `db.json`

```json
{
  "organizations": [],
  "users": [],
  "groups": [],
  "reservoirs": [],
  "devices": [],
  "operatorProfiles": [],
  "deviceAssignments": [],
  "firstAccessCodes": [],
  "configurations": [],
  "measurements": [],
  "processes": [],
  "actuations": [],
  "alerts": [],
  "incidents": [],
  "traceabilityEntries": [],
  "reports": []
}
```

### Relaciones simuladas

- `organizations.id` identifica la frontera empresarial de los datos.
- `users.organizationId` vincula cada Administrador u Operario con una sola organización.
- Las entidades operativas incluyen `organizationId`; ninguna relación puede unir recursos de organizaciones distintas.
- `groups.id` identifica el único grupo de cada Operario.
- `reservoirs.groupId` vincula el reservorio con su grupo.
- `devices.reservoirId` vincula de manera exclusiva un dispositivo con un reservorio.
- `operatorProfiles.userId` y `operatorProfiles.groupId` vinculan la cuenta con su único grupo.
- `deviceAssignments.operatorProfileId` vincula uno o varios pares reservorio-dispositivo con el Operario responsable.
- Una restricción del mock debe impedir dos asignaciones activas con el mismo `deviceId` o `reservoirId`.
- `firstAccessCodes.operatorProfileId` vincula el código único con el perfil completo.
- `configurations.deviceId` vincula configuración y dispositivo.
- `measurements.deviceId` vincula mediciones y dispositivo.
- `processes.deviceId` vincula el proceso vigente.
- `actuations.processId` y `actuations.cycleNumber` identifican la actuación.
- Alertas, incidentes y trazabilidad utilizan `deviceId`, `processId` y `correlationId` cuando corresponda.

### Reglas del mock

- Todos los identificadores deben ser cadenas para evitar diferencias entre Dart y TypeScript.
- Las fechas se expresan en UTC e ISO 8601.
- Las contraseñas incluidas son exclusivamente ficticias.
- El mock de autenticación puede validar usuarios y códigos falsos, pero no representa seguridad real.
- La empresa se obtiene de la sesión autenticada. Las rutas protegidas no deben confiar en un `organizationId` enviado por el cliente para ampliar su alcance.
- El registro de organización y Administrador debe simular una operación indivisible, validar RUC y correo únicos y conservar la relación de un Administrador por organización.
- El código de primer acceso no vence en el prototipo, pero solo puede cambiar de `ACTIVE` a `USED` o `REVOKED`.
- El servidor mock solo implementa las validaciones necesarias para probar los contratos frontend. Sus reglas no constituyen evidencia de que el dominio productivo esté implementado correctamente.
- Los comandos críticos pueden simularse actualizando el recurso correspondiente, pero la aplicación no debe asumir que podrá hacerlo directamente en producción.
- No se codificarán URLs de localhost dentro de widgets o componentes; cada aplicación utilizará configuración de entorno.

### Escenarios iniciales de datos

El `db.json` debe contener, como mínimo:

- Dos organizaciones de demostración con RUC y correo distintos, un único Administrador cada una y recursos aislados mediante `organizationId`.
- Un grupo con un Operario y otro grupo con varios Operarios, siempre dentro de sus organizaciones respectivas.
- Un Operario con un reservorio-dispositivo y otro con varias asignaciones dentro del mismo grupo.
- Un Administrador sin dispositivo por organización.
- Un perfil pendiente de primer acceso, uno activo y uno desactivado.
- Un código activo, uno utilizado y uno revocado.
- Un reservorio-dispositivo sin responsable después de una desvinculación.
- Un dispositivo en línea y otro sin comunicación.
- Configuración publicada y una configuración en borrador.
- Mediciones conformes y no conformes.
- Un proceso en corrección, otro en espera y otro en estado listo.
- Actuación completada, rechazada y fallida.
- Alerta activa, atendida y resuelta.
- Incidente con trazabilidad completa.
- Reporte disponible y solicitud de reporte pendiente.

## 7. Autenticación y autorización entre aplicaciones

- La app móvil solo acepta una sesión con rol `OPERATOR`.
- La app web solo acepta una sesión con rol `ADMINISTRATOR`.
- Existe registro público únicamente para crear una organización y su única cuenta administradora.
- El Administrador crea las cuentas de Operario con una contraseña definitiva; no existe autorregistro público de Operarios.
- El primer acceso móvil se realiza directamente con el código único y no solicita cambio de contraseña.
- El código no caduca por tiempo, pero es de un solo uso y puede ser revocado antes de utilizarse.
- Después del primer acceso, el Operario ingresa con el identificador y la contraseña definitiva entregados por el Administrador.
- Si el usuario intenta ingresar en la aplicación incorrecta, se cierra la sesión iniciada y se informa qué canal le corresponde.
- El token se conserva en memoria y en almacenamiento seguro o de sesión según la plataforma.
- Flutter utilizará almacenamiento seguro cuando se incorpore persistencia real de credenciales.
- Angular podrá usar `sessionStorage` para el token durante el prototipo; no se almacenarán contraseñas.
- Las restricciones de la interfaz no sustituyen la autorización del backend.
- La sesión autenticada incluye `organizationId`, pero la autorización del backend lo deriva del token o sesión verificada y no de un valor manipulable enviado por el cliente.

## 8. Aplicación móvil del Operario — Flutter con BLoC

### Arquitectura general

```text
lib/
├── app/
│   ├── app.dart
│   ├── router/
│   └── theme/
├── core/
│   ├── auth/
│   ├── config/
│   ├── errors/
│   ├── http/
│   └── widgets/
├── features/
│   ├── identity_access/
│   ├── device_configuration/
│   ├── telemetry/
│   ├── treatment_release/
│   └── monitoring_traceability/
└── main.dart
```

Cada feature debe separar:

```text
feature/
├── data/
│   ├── datasources/
│   ├── dto/
│   ├── mappers/
│   └── repositories/
├── domain/
│   ├── entities/
│   ├── repositories/
│   └── usecases/
└── presentation/
    ├── bloc/
    ├── pages/
    └── widgets/
```

La UI emite eventos BLoC, el BLoC ejecuta un caso de uso y el repositorio selecciona el datasource remoto basado en Retrofit o el datasource mock. Los widgets, BLoC y casos de uso no llaman directamente a Retrofit, Dio ni `db.json`.

### Navegación móvil

- Inicio.
- Mis reservorios.
- Mi grupo.
- Proceso.
- Alertas.
- Historial.
- Configuración.
- Perfil y cierre de sesión.

La pantalla inicial muestra un selector cuando existe más de un reservorio asignado. Después de seleccionar uno, prioriza el estado de su proceso, última medición, actuación vigente y emergencia. Las acciones frecuentes deben poder ejecutarse con una mano y no depender de tablas anchas.

### M-BC-01 — Identity and Access Management

**Función:** efectuar el primer acceso con el código emitido por el Administrador, autenticar posteriormente al Operario y mantener una sesión limitada a su perfil y asignaciones.

**Acciones:**

- Iniciar la primera sesión mediante el código único.
- Mostrar grupo, reservorios y dispositivos antes de confirmar el primer acceso.
- Continuar a la aplicación sin solicitar cambio de contraseña.
- Iniciar las sesiones posteriores con el identificador y la contraseña definitiva.
- Cerrar sesión.
- Restaurar la sesión válida.
- Consultar datos básicos del perfil.
- Consultar los nombres de los integrantes del mismo grupo y sus reservorios asignados, sin acceso a información operativa ajena.
- Informar cuando el perfil no tenga asignaciones activas.
- Rechazar en la app móvil cuentas con rol de Administrador.

**Pantallas y BLoC:**

- `FirstAccessPage` con `FirstAccessBloc`.
- `LoginPage` con `AuthenticationBloc` para accesos posteriores.
- `OperatorProfilePage` con `OperatorProfileBloc`.
- `GroupMembersPage` con `GroupMembersBloc`.
- Estados: `initial`, `submitting`, `authenticated`, `unauthenticated`, `forbidden` y `failure`.

**No incluye:** autorregistro, creación de cuentas, asignación de roles ni administración de otras personas.

### M-BC-02 — Device and Operational Configuration

**Función:** permitir que el Operario complete y mantenga la configuración de cada reservorio-dispositivo incluido en su perfil.

**Acciones:**

- Seleccionar una asignación cuando gestione varios reservorios dentro de su grupo.
- Consultar grupo, reservorio, dispositivo, entorno, capacidades y versión vigente.
- Editar rangos de pH y temperatura.
- Configurar sustancia o acción, dosis o intensidad y estrategia térmica.
- Configurar tiempo de espera y límite de ciclos.
- Elegir modo de liberación manual o automático.
- Revisar un resumen y publicar la nueva versión.
- Impedir cambios sobre la versión utilizada por un proceso activo.
- Mostrar incompatibilidades entre estrategia y capacidades.

**Pantallas y BLoC:**

- `ConfigurationSummaryPage` con `ConfigurationDetailsBloc`.
- `ConfigurationEditPage` con `ConfigurationFormBloc`.
- Formulario dividido en pasos cortos y aptos para móvil.

**Validaciones de presentación:** pH entre 0 y 14, temperatura entre 0 °C y 100 °C, límites coherentes, espera y ciclos positivos, y datos de estrategia completos.

**No incluye:** registrar dispositivos, reasignar Operarios ni crear perfiles base para toda la organización.

### M-BC-03 — IoT Telemetry and Device Integration

**Función:** presentar al Operario el estado técnico y las mediciones de cada dispositivo que tenga asignado.

**Acciones:**

- Seleccionar uno de sus reservorios-dispositivos sin mezclar datos entre asignaciones.
- Consultar pH, temperatura, unidad y fecha de la última medición.
- Identificar dato vigente, retrasado o inexistente.
- Consultar disponibilidad y última comunicación.
- Mostrar entorno y capacidades.
- Consultar historial reciente por periodo.
- Mostrar el resultado técnico de actuaciones y válvula.
- Actualizar mediante polling mientras no exista SSE o WebSocket.

**Pantallas y BLoC:**

- `DeviceOverviewPage` con `DeviceStatusBloc`.
- `TelemetryPage` con `TelemetryBloc`.
- `CommandStatusPage` con `CommandExecutionBloc`.

**Comportamiento:** el polling se detiene cuando la app queda en segundo plano. Una ausencia de datos se muestra como “Sin medición reciente”, nunca como cero.

**No incluye:** registrar mediciones manualmente ni clasificar el agua desde la app.

### M-BC-04 — Water Quality Treatment and Release

**Función:** permitir la intervención operativa directa y segura sobre el proceso del reservorio seleccionado.

**Acciones:**

- Iniciar un proceso si el reservorio seleccionado posee configuración publicada y dispositivo disponible.
- Consultar estado, ciclo, máximo de ciclos y espera restante.
- Ver la actuación correctiva ordenada y su estado técnico.
- Confirmar la liberación únicamente cuando el modo sea manual y el proceso esté `READY`.
- Observar la liberación automática sin convertirla en una acción manual.
- Activar la parada de emergencia desde cualquier estado activo.
- Solicitar restablecimiento después de atender un fallo o emergencia.
- Mostrar la sustitución manual solo en el prototipo académico.

**Pantallas y BLoC:**

- `TreatmentProcessPage` con `TreatmentProcessBloc`.
- `ManualReleaseSheet` con `ReleaseCommandBloc`.
- `EmergencyStopDialog` con `EmergencyCommandBloc`.
- `ResetProcessPage` con `ResetProcessBloc`.

**Eventos BLoC principales:**

- `TreatmentStarted`
- `TreatmentRefreshed`
- `ManualReleaseRequested`
- `EmergencyStopRequested`
- `ProcessResetRequested`

**Reglas de interfaz:**

- No incrementar ciclos localmente.
- No cambiar a `READY` cuando termina visualmente un contador.
- No mostrar la dosificación normal como tarea manual del Operario.
- En `ACADEMIC_PROTOTYPE`, explicar que el LED representa la orden mientras el equipo realiza la corrección sustitutiva.
- Deshabilitar una acción hasta recibir el resultado del comando anterior.

### M-BC-05 — Operational Monitoring and Traceability

**Función:** permitir al Operario atender alertas y revisar la historia de sus reservorios-dispositivos.

**Acciones:**

- Consultar alertas activas, atendidas y resueltas de sus dispositivos asignados.
- Filtrar por severidad, estado y periodo.
- Ver la causa, proceso, ciclo y medición relacionados.
- Marcar una alerta como atendida cuando el contrato lo permita.
- Consultar la línea temporal de mediciones, actuaciones, ciclos y liberaciones.
- Consultar reportes disponibles de su dispositivo, sin administrar reportes de toda la organización.

**Pantallas y BLoC:**

- `AlertsPage` y `AlertDetailPage` con `OperatorAlertsBloc`.
- `DeviceHistoryPage` con `TraceabilityBloc`.
- `AvailableReportsPage` con `OperatorReportsBloc`.

**No incluye:** registrar incidentes administrativos, consultar datos operativos de otros integrantes del grupo ni generar reportes de toda la organización.

## 9. Aplicación web del Administrador — Angular con DDD por bounded context

### Arquitectura general

```text
src/app/
├── core/
│   ├── auth/
│   ├── config/
│   ├── http/
│   ├── layout/
│   └── error-handling/
├── shared/
│   ├── presentation/
│   └── utils/
├── contexts/
│   ├── identity-access/
│   ├── device-configuration/
│   ├── telemetry/
│   ├── treatment-release/
│   └── monitoring-traceability/
├── app.config.ts
└── app.routes.ts
```

Estructura interna por contexto:

```text
bounded-context/
├── domain/
│   ├── models/
│   └── ports/
├── application/
│   ├── use-cases/
│   └── state/
├── infrastructure/
│   ├── axios/
│   ├── dto/
│   └── mappers/
└── presentation/
    ├── pages/
    ├── components/
    └── routes.ts
```

Los componentes usan casos de uso; los casos de uso dependen de puertos; y los adaptadores Axios implementan esos puertos. Ningún contexto accede al adaptador interno de otro. Los dashboards compuestos consumen fachadas de aplicación o vistas preparadas.

### Navegación web

- Resumen de la organización.
- Operarios y perfiles.
- Grupos y reservorios.
- Dispositivos y asignaciones.
- Códigos de primer acceso.
- Configuraciones publicadas.
- Telemetría de la organización.
- Procesos.
- Alertas e incidentes.
- Historial y reportes.
- Perfil y cierre de sesión.

### W-BC-01 — Identity and Access Management

**Función:** registrar una organización con su único Administrador, autenticarlo y permitirle crear, activar y desactivar las cuentas de Operario y sus códigos de primer acceso dentro de su empresa.

**Acciones:**

- Registrar públicamente una organización junto con su única cuenta administradora.
- Validar la unicidad del RUC y del correo del Administrador.
- Iniciar y cerrar sesión.
- Listar, buscar y filtrar únicamente los usuarios de la organización autenticada.
- Crear la cuenta de un Operario con identificador y contraseña definitiva.
- Consultar estado activo o inactivo.
- Generar el código únicamente después de comprobar que el perfil, grupo y al menos un reservorio-dispositivo estén completos.
- Consultar si el código está activo, utilizado o revocado.
- Revocar un código sin utilizar y generar su reemplazo.
- Desactivar una cuenta mediante eliminación lógica.
- Consultar el vínculo entre Operario, grupo y asignaciones sin modificar esos datos desde IAM.
- Rechazar en la web administrativa una sesión exclusiva de Operario.

**Rutas y componentes:**

- `/login` — `AdminLoginPage`.
- `/register` — `AdministratorRegisterPage`.
- `/users` — `UsersPage` y `UsersTable`.
- `/users/new` — `OperatorCreatePage`.
- `/users/:userId` — `UserDetailPage`.
- `/users/:userId/first-access` — `FirstAccessCodePage`.

### W-BC-02 — Device and Operational Configuration

**Función:** administrar grupos, reservorios, dispositivos y perfiles de Operario, y supervisar las configuraciones que cada Operario publica desde Flutter.

**Acciones:**

- Crear grupos con nombre, propósito y tipo de cultivo o proceso; el segmento se hereda de la organización y no se vuelve a seleccionar.
- Registrar reservorios dentro de un grupo.
- Registrar y activar dispositivos con número de serie, modelo, entorno y capacidades.
- Vincular exactamente un dispositivo con cada reservorio.
- Listar pares reservorio-dispositivo asignados y sin responsable.
- Crear el Perfil de Operario y asociarlo con un único grupo.
- Agregar uno o varios pares disponibles del mismo grupo al Perfil de Operario.
- Impedir que un dispositivo o reservorio tenga dos responsables activos.
- Desvincular una asignación y dejar el par sin responsable.
- Reasignar manualmente un par disponible desde el perfil del nuevo Operario.
- Consultar la configuración vigente y su versión.
- Detectar dispositivos sin configuración o con configuración incompatible.
- Consultar borradores y versiones publicadas.
- No modificar silenciosamente una configuración operativa activa del Operario.
- Desactivar grupos, reservorios, dispositivos y perfiles mediante eliminación lógica.

**Rutas y componentes:**

- `/groups` — `WorkGroupsPage`.
- `/groups/new` — `WorkGroupCreatePage`.
- `/groups/:groupId` — `WorkGroupDetailPage` con Operarios y reservorios.
- `/reservoirs/new` — `ReservoirCreatePage`.
- `/reservoirs/:reservoirId` — `ReservoirDetailPage`.
- `/devices` — `DevicesPage`.
- `/devices/new` — `DeviceCreatePage`.
- `/devices/:deviceId` — `DeviceDetailPage`.
- `/operator-profiles` — `OperatorProfilesPage`.
- `/operator-profiles/:profileId` — `OperatorProfileDetailPage` y `AssignmentManager`.
- `/devices/:deviceId/configurations` — `ConfigurationVersionsPage`.

La creación completa del Operario se presenta como un flujo guiado de la aplicación web: cuenta y contraseña definitiva → Perfil de Operario → grupo → reservorio-dispositivo → código. La interfaz coordina los casos de uso de IAM y Configuration, pero no mezcla sus modelos internos ni escribe directamente en sus bases de datos.

### W-BC-03 — IoT Telemetry and Device Integration

**Función:** supervisar la disponibilidad y telemetría de todos los dispositivos de la organización sin ejecutar acciones operativas propias del Operario.

**Acciones:**

- Consultar última medición por dispositivo.
- Filtrar por disponibilidad y entorno dentro de la organización; el segmento se obtiene de la empresa autenticada.
- Identificar dispositivos en línea, retrasados o fuera de línea.
- Consultar historial de pH y temperatura por dispositivo y periodo.
- Consultar resultados técnicos de comandos.
- Comparar estados mediante tarjetas y tablas, sin recalcular conformidad.

**Rutas y componentes:**

- `/telemetry` — `TelemetryOverviewPage`.
- `/telemetry/devices/:deviceId` — `DeviceTelemetryDetailPage`.
- `/telemetry/devices/:deviceId/commands` — `DeviceCommandsPage`.

**Restricción:** el Administrador puede observar la actuación y la válvula, pero la pantalla no presenta botones de inicio, liberación manual o emergencia.

### W-BC-04 — Water Quality Treatment and Release

**Función:** permitir la supervisión de los procesos de la organización y explicar las decisiones tomadas por el sistema.

**Acciones:**

- Listar procesos por dispositivo, estado y periodo dentro de la organización; el segmento se obtiene de la empresa autenticada.
- Consultar proceso activo, ciclo, configuración y actuación.
- Ver la medición que originó una corrección.
- Ver la reevaluación que habilitó una liberación.
- Consultar fallos, emergencias y restablecimientos.
- Ver autorizaciones de liberación y resultado de válvula.
- Navegar hacia alertas, incidentes y trazabilidad relacionados.

**Rutas y componentes:**

- `/processes` — `ProcessesPage`.
- `/processes/:processId` — `ProcessDetailPage`.
- `/processes/:processId/timeline` — `ProcessTimelinePage`.

**Restricción:** esta aplicación presenta información de supervisión. Las acciones operativas directas corresponden a la app móvil del Operario.

### W-BC-05 — Operational Monitoring and Traceability

**Función:** centralizar alertas, incidentes, trazabilidad y reportes de todos los dispositivos de la organización.

**Acciones:**

- Listar y filtrar alertas de la organización.
- Consultar alertas críticas y pérdida de monitoreo.
- Consultar y administrar incidentes.
- Relacionar visualmente medición, actuación, ciclo, alerta y liberación usando la proyección recibida.
- Generar reportes por dispositivo y periodo.
- Consultar estado de generación.
- Exportar reportes disponibles.
- Acceder desde un incidente al proceso y dispositivo correspondientes.

**Rutas y componentes:**

- `/alerts` — `OrganizationAlertsPage`.
- `/alerts/:alertId` — `AlertDetailPage`.
- `/incidents` — `IncidentsPage`.
- `/incidents/:incidentId` — `IncidentDetailPage`.
- `/traceability/devices/:deviceId` — `DeviceTraceabilityPage`.
- `/reports` — `ReportsPage`.

## 10. Experiencia visual propuesta

### Flutter

- Navegación inferior para Inicio, Proceso, Alertas e Historial.
- Configuración y perfil dentro de un menú secundario.
- Tarjetas grandes para pH, temperatura, estado y ciclo.
- Botón de emergencia visible durante procesos activos, pero protegido por confirmación.
- Bottom sheets para liberación manual y acciones breves.
- Textos operativos cortos y estados legibles sin depender solo del color.

### Angular

- Sidenav responsive con navegación administrativa.
- Tablas con ordenamiento, filtros y paginación.
- Formularios con Reactive Forms.
- Diálogos de confirmación para asignaciones y cambios de rol.
- Tarjetas de resumen para disponibilidad, procesos y alertas.
- Breadcrumbs para mantener contexto al navegar entre dispositivo, proceso e incidente.

## 11. Integración HTTP

### Flutter con Retrofit

- Definir una interfaz `HydroGuardApiClient` anotada con `@RestApi`, dividida en clientes por bounded context cuando el contrato crezca.
- Declarar cada endpoint mediante las anotaciones de Retrofit (`@GET`, `@POST`, `@PUT`, `@PATCH` y `@DELETE`) y utilizar DTO tipados.
- Generar la implementación con `retrofit_generator` y `build_runner`; los archivos generados no se editan manualmente.
- Serializar y deserializar DTO mediante `json_serializable`, manteniendo separados los DTO y las entidades del dominio móvil.
- Utilizar una instancia Dio únicamente como transporte interno configurado por ambiente para `baseUrl`, tiempo de espera e interceptores.
- Incorporar en Dio los encabezados de token, correlación y versión del cliente antes de entregar la instancia a Retrofit.
- Convertir centralmente `DioException` y respuestas HTTP en fallos propios de la aplicación antes de llegar al BLoC.
- Cancelar el polling cuando la aplicación quede en segundo plano.
- Permitir que los repositorios alternen entre `RetrofitRemoteDataSource` y `MockDataSource` sin modificar widgets ni BLoC.
- Evitar cualquier llamada manual con `dio.get`, `dio.post` u operaciones equivalentes fuera de la infraestructura generada por Retrofit.

### Angular con Axios

- Una instancia Axios común configurada mediante `environment`.
- Interceptor de autenticación y `X-Correlation-Id`.
- Tratamiento uniforme de `401`, `403`, `404`, `409`, `422` y `5xx`.
- Cancelación de búsquedas o consultas reemplazadas.
- Adaptadores Axios fuera de los componentes.

### Política de errores compartida

- `401`: cerrar sesión y volver al login.
- `403`: mantener sesión y mostrar acceso denegado.
- `404`: indicar que el recurso ya no existe.
- `409`: recargar el estado y explicar el conflicto.
- `422`: asociar errores a campos o datos enviados.
- Error de red: conservar información anterior y permitir reintento.

## 12. Endpoints de consulta y comando requeridos

El backend real aún debe completar varios contratos. El mock debe anticipar estas necesidades sin presentarlas como implementación definitiva.

| Contexto | App móvil del Operario | App web del Administrador |
|:--|:--|:--|
| IAM | Primer acceso directo por código, login posterior con credenciales definitivas y perfil actual. | Registro transaccional de organización y Administrador, login por correo, cuentas de Operario, códigos, revocación y eliminación lógica. |
| Configuration | Grupo propio, miembros visibles, asignaciones, configuración por reservorio-dispositivo y publicación. | Grupos, reservorios, dispositivos, perfiles, vinculación, asignación, desvinculación y versiones. |
| Telemetry | Última medición, historial, disponibilidad y comandos de cada dispositivo propio. | Resumen de la organización, detalle por dispositivo y filtros. |
| Treatment | Proceso activo, inicio, liberación manual, emergencia y restablecimiento. | Listado de procesos de la organización y detalle en modo consulta. |
| Monitoring | Alertas e historial del dispositivo asignado. | Alertas de la organización, incidentes, trazabilidad, reportes y exportación. |

### Brechas que deben corregirse antes de conectar el backend real

- El backend debe exponer `POST /api/v1/organization-registrations` para crear de forma transaccional una organización y su único Administrador, validando RUC y correo únicos.
- La respuesta de autenticación debe incluir rol, vigencia de sesión y el `organizationId` autorizado.
- Todas las operaciones protegidas deben obtener la organización desde la sesión y rechazar referencias a recursos de otra empresa.
- Se necesita un endpoint transaccional de primer acceso que consuma el código, active el perfil e inicie la sesión sin modificar la contraseña definitiva.
- El código debe poder consultarse por estado, revocarse y regenerarse antes de utilizarse; no requiere fecha de caducidad en el alcance actual.
- Se necesitan consultas paginadas de usuarios y dispositivos.
- Los contratos deben representar grupos, reservorios, perfiles de Operario y asignaciones con las cardinalidades establecidas.
- La API debe garantizar un solo grupo por Operario, un dispositivo por reservorio y un solo responsable activo por dispositivo.
- La API debe garantizar un único segmento por organización; grupos, perfiles y dispositivos lo heredan y no pueden redefinirlo.
- La creación del código debe rechazarse mientras el perfil no tenga grupo y al menos una asignación reservorio-dispositivo completa.
- Los rangos deben expresarse como pH y temperatura; no como `minVma`, `maxVma`, `minCrop` y `maxCrop`.
- El DTO inicial de telemetría no debe exigir turbidez, porque está fuera del alcance actual.
- La evaluación de una medición es una interacción entre backend y telemetría, no un comando emitido desde las aplicaciones.
- La vista de proceso debe entregar las acciones permitidas para no duplicar las reglas en Flutter o Angular.
- Monitoring necesita endpoints de consulta; los endpoints internos de creación de alertas y correlación no deben ser invocados por las aplicaciones.

## 13. Estados de presentación

Ambas aplicaciones deben distinguir:

- `initial`
- `loading`
- `refreshing`
- `success`
- `empty`
- `forbidden`
- `notFound`
- `offline`
- `failure`

Para comandos deben utilizarse estados separados:

- `ready`
- `submitting`
- `accepted`
- `completed`
- `rejected`
- `failed`

## 14. Orden general de desarrollo

### P-01 — Preparación compartida

- Acordar contratos y enumeraciones.
- Incorporar organización, registro empresa-Administrador y aislamiento por `organizationId`.
- Incorporar grupo, reservorio, Perfil de Operario, asignación y código de primer acceso.
- Mantener `mock-api/server.mjs`, `mock-api/db.json` y escenarios iniciales compatibles con el backend real.
- Definir variables de entorno para ambas aplicaciones.

### P-02 — Base Flutter

- Crear proyecto, tema y navegación; configurar Retrofit, Dio como transporte interno, serialización y generación de código.
- Incorporar BLoC y estructura por feature.
- Implementar autenticación de Operario.

### P-03 — Base Angular

- Crear proyecto, tema Angular Material, layout y Router.
- Configurar Axios y estructura DDD por contexto.
- Implementar registro de organización y Administrador, seguido de autenticación por correo.

### P-04 — Configuración por rol

- Flutter: selector de reservorio y configuración operativa de cada asignación.
- Angular: grupos, reservorios, dispositivos, perfiles de Operario, códigos y asignaciones exclusivas.

### P-05 — Telemetría y tratamiento

- Flutter: telemetría propia y acciones operativas.
- Angular: telemetría y procesos de la organización en modo supervisión.

### P-06 — Monitoreo y trazabilidad

- Flutter: alertas e historial del dispositivo.
- Angular: alertas, incidentes, trazabilidad y reportes de la organización.

### P-07 — Refinamiento

- Revisar navegación, textos, responsive y estados excepcionales.
- Recoger observaciones de uso.
- Trasladar a Figma únicamente los flujos aprobados si el equipo decide formalizarlos.

## 15. Criterio de finalización del sprint

Una funcionalidad se considera completada cuando:

- Está implementada en la aplicación correspondiente al rol.
- No expone acciones reservadas a la otra aplicación.
- Utiliza el patrón definido: BLoC en Flutter o DDD por contexto en Angular.
- Consume un repositorio o puerto; la UI nunca accede directamente a Retrofit, Dio, Axios o `db.json`.
- Puede funcionar con el mock sin cambiar componentes o widgets.
- Incluye carga, vacío, desconexión, error y falta de permisos.
- Impide dobles envíos y confirma acciones críticas.
- Mantiene nombres y estados coherentes con el README.
- Funciona en los tamaños de pantalla previstos para su aplicación.
- Documenta el recurso mock o endpoint utilizado.
- Impide visualizar o modificar datos pertenecientes a otra organización.

Las pruebas unitarias y la landing page permanecen fuera del sprint actual.

## 16. Decisiones pendientes de refinamiento

- Versiones exactas de Angular, Flutter y sus bibliotecas.
- Repositorios definitivos y estrategia de despliegue de cada aplicación.
- URL, autenticación y OpenAPI del backend real.
- Intervalo de polling y criterio de medición vigente.
- Uso posterior de SSE o WebSocket.
- Persistencia final de sesión en Flutter.
- Biblioteca de gráficos, solo si se demuestra que aporta valor frente a tarjetas y tablas.
- Diseños que deberán formalizarse en Figma después de recibir observaciones.
