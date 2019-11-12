# Adaptone-firmware-sonde

## Configuration d'un poste de travail
1. Installer Visual Studio Code
2. Installer le plugin PlatformIO

## Compiler le projet
1. Ouvrir le projet dans Visual Studio Code à l'aide de l'onglet PlatformIO
2. Modifier le fichier `.platformio/packages/framework-espidf/components/lwip/lwip/src/include/lwip/apps/sntp_opts.h`
Mettre en commentaire ces 3 lignes.
```c
/** SNTP macro to change system time in seconds
 * Define SNTP_SET_SYSTEM_TIME_US(sec, us) to set the time in microseconds instead of this one
 * if you need the additional precision.
 */
#if !defined SNTP_SET_SYSTEM_TIME || defined __DOXYGEN__
#define SNTP_SET_SYSTEM_TIME(sec)   LWIP_UNUSED_ARG(sec)
#endif
```
Mettre 15000 au lieu de 3600000.
```c
/** SNTP update delay - in milliseconds
 * Default is 1 hour. Must not be beolw 15 seconds by specification (i.e. 15000)
 */
#if !defined SNTP_UPDATE_DELAY || defined __DOXYGEN__
#define SNTP_UPDATE_DELAY           15000
#endif
```
3. Cliquer sur le bouton `build`
