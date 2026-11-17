# Xash3D FWGS — PlayStation 4 Port (GoldHEN)

Puerto nativo del motor Xash3D FWGS adaptado para la consola PlayStation 4. Este proyecto está diseñado para ofrecer una experiencia de juego fluida, controles ergonómicos de consola y soporte multijugador con mods avanzado.

## Actualizaciones y Novedades de este port

El motor ha sido adaptado para introducir capacidades multijugador avanzadas en la consola:

- Soporte Split-Screen en LAN: Se ha implementado con éxito la funcionalidad de pantalla dividida local y en red LAN, permitiendo conectar múltiples jugadores y unirse a partidas locales de forma fluida.
- Split-Screen para 3 y 4 Jugadores (Experimental): El soporte para sesiones simultáneas de 3 y 4 jugadores se encuentra actualmente en fase de pruebas / experimental y no funciona de forma estable por ahora.
- Split-Screen en Modo Online (En Desarrollo): La arquitectura para extender la pantalla dividida a sesiones de juego remoto a través de internet se encuentra en fase experimental y se está trabajando activamente en su optimización.
- Capacidad de jugar con jugadores de PC y Android.
- Adaptación completa para leer mods de PC y jugar en servidores con mods.

---

## Controles y Mapeo del Mando de PS4 (DualShock 4)

El motor cuenta con un mapeo de controles totalmente adaptado para aprovechar cada función del mando de PlayStation 4:

D-Pad

Arriba: soltar arma en mano

Abajo: Elegir rol terrorist o counter terrorist

Izquierda: abrir menu de compra

Derecha: Graffiti

- Stick Analógico Izquierdo (`L3`): Movimiento del personaje (caminar, correr).
- Stick Analógico Derecho (`R3`): Control de la cámara y vista libre. Al presionar el stick R3 se despliega una rueda de números, útil para seleccionar cosas en el menú del port. como elegir equipo selecionar mapas o comprar armas 
- Giroscopio (Sensor de Movimiento): Integrado de forma fluida. Presiona `L3` + `R3` al mismo tiempo para activarlo.
- Touchpad (Panel táctil): Al mover el Touchpad aparece un mini mouse que fue adaptado para moverse libremente en los menús. Puedes también usarlo en el juego para apuntar y hacer clic para disparar; si se dan dos clics seguidos se activa la acción del clic derecho de un mouse.
- Botones de Acción (`▲`, `■`, `●`, `✕`): 
  - `✕`: Saltar / Confirmar en menús.
  - `■`: Recargar arma.
  - `●`: Agacharse (duck).
  - `▲`: Interactuar con objetos del entorno.
- Gatillos Superiores e Inferiores (`L1`, `R1`, `L2`, `R2`):
  - `R1`: Cambiar arma a la derecha.
  - `L1`: Cambiar arma a la izquierda.
  - `L2`: Acción especial de arma (clic derecho).
  - `R2`: Disparar (clic izquierdo).
- Botón OPTIONS: Abre el menú principal de pausa del juego.

Todos estos controles vienen por defecto ya configurados, pero si gustas puedes cambiarlos a tu preferencia desde el menú de configuraciones del juego.

Compatibilidad Completa con Teclado Y mouse

---

## Instrucciones de Instalación en PlayStation 4

Para instalar y poner en marcha este port en tu consola con GoldHEN, sigue estos pasos:

1. **Prepara los archivos de juego:** 
   Asegúrate de contar con los archivos base necesarios de tus juegos (como las carpetas `valve` o `cstrike`) y transfiérelos al almacenamiento de tu consola utilizando una memoria USB o mediante conexión FTP en la ruta correspondiente para Xash3D (`/data/xash/`).
2. **Instala el paquete de la aplicación:**
   Descarga o compila el archivo en formato `.pkg` correspondiente a esta versión del puerto.
3. **Ejecuta en tu PS4:**
   Instala el archivo `.pkg` mediante el instalador de paquetes de GoldHEN en tu PlayStation 4 y lanza la aplicación desde el menú principal.

---

## Notas Misceláneas Importantes

- **Ruta de Datos:** Recuerda que el motor busca automáticamente los directorios de los juegos en la ruta interna `/data/xash/`. Verifica que tus carpetas de assets estén bien ubicadas para evitar errores de lectura.
- **Compatibilidad con Servidores:** Gracias al soporte multijugador multiplataforma, puedes unirte tanto a servidores dedicados clásicos de PC y Android como a partidas locales creadas con otros dispositivos compatibles.
**Proyecto en fase de pruebas:** Cabe aclarar que este proyecto contiene algunos bugs que se corregiran y adaptaran a la ps4 con el tiempo, Tambien  aclarar que este proyecto esta trabajado para Counter strike 2 el half life es jugable pero aun no se adapta el modo multijugador y otras funciones,
---

## Licencia y Atribución

Este proyecto está basado en el motor de código abierto Xash3D FWGS (creado originalmente por Unkle Mike y mantenido por su comunidad). Se distribuye bajo los términos de la licencia GNU GPL, respetando las autorías originales del motor y añadiendo las adaptaciones exclusivas para PlayStation 4 desarrolladas en este repositorio.
