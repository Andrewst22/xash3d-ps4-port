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

- **Ruta de Datos:** Recuerda que el motor busca automáticamente los directorios de los juegos en la ruta interna `/data/xash/`. Verifica que tus carpetas de assets estén bien ubicadas para evitar errores de lectura. la carpeta xash en la ruta de /data de ps4 no existe por defecto debes crearla tu mismo y alli dentro poner las carpetas de valve y cstrike de tu copia original de steam
- **Compatibilidad con Servidores:** Gracias al soporte multijugador multiplataforma, puedes unirte tanto a servidores dedicados clásicos de PC y Android como a partidas locales creadas con otros dispositivos compatibles.
**Proyecto en fase de pruebas:** Cabe aclarar que este proyecto contiene algunos bugs que se corregiran y adaptaran a la ps4 con el tiempo, Tambien  aclarar que este proyecto esta trabajado para Counter strike 2 el half life es jugable pero aun no se adapta el modo multijugador y otras funciones,
---

##SCREENSHOTS DEL PORT
Menu del juego 
<img width="3840" height="2160" alt="Xash3D FWGS_20261118163952" src="https://github.com/user-attachments/assets/10d15840-24e4-4c41-9e99-6362f1675997" />

Para Activar el modo split screen en lan debes mover el mouse al apartado split screen y darle d-pad derecha y se selecciona al 2, puedes intentar jugar de 3 o 4 jugadores pero esa fase del desarrollo esta en alfa no es funcional y se esta trabajando para implementarlo por ahora solo se puede jugar de 2 jugadores, al crear la partida en lan puedes unirte desde cualquier plataforma ya sea android o pc y veras que desde la ps4 se transmiten 2 jugadores correctamente 
<img width="3840" height="2160" alt="Xash3D FWGS_20261118165947" src="https://github.com/user-attachments/assets/5b5eada4-68e6-486b-8cc7-fd2fb4894bdc" />

Muestra de la rueda de numeros para compras desplazarse por menus etc, Esta se activa pulsando R3 (stick derecho)
<img width="3840" height="2160" alt="Xash3D FWGS_20261118164235" src="https://github.com/user-attachments/assets/390d9ffa-aef6-4eb0-92ad-1c5098e5bd84" />

modo espectador 
<img width="3840" height="2160" alt="Xash3D FWGS_20261118164400" src="https://github.com/user-attachments/assets/a1d335a1-88cf-48e6-b3bc-bae77ab8f2c5" />

Demostracion de carga de mods correcta y servidores de la comunidad implementado 
<img width="3840" height="2160" alt="Xash3D FWGS_20261118164432" src="https://github.com/user-attachments/assets/61bd55f0-d3b6-469b-aa3f-d4d5bf597ad7" />



## Licencia y Atribución

Este proyecto está basado en el motor de código abierto Xash3D FWGS (creado originalmente por Unkle Mike y mantenido por su comunidad). Se distribuye bajo los términos de la licencia GNU GPL, respetando las autorías originales del motor y añadiendo las adaptaciones exclusivas para PlayStation 4 desarrolladas en este repositorio.
