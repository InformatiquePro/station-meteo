# meteo
Une application de météo pour l'esp8266-oled (HW-364A), adapté et amélioré depuis un autre projet existant.(certaines fonctionnalite ne sont que ici et non sur cette autre projet)
*Ce projet est basé sur [CE PROJET](https://github.com/datatomotion/Station-M-t-o-Ultime-V1)*

# Utilisation
ETAPE 1 : Penser à bien personnaliser dans le fichier Station_meteo.ino les lignes 21, 22, 26 et 27 avec votre wifi, votre id de ville OpenWeather et votre cle d'api OpenWeather, vous pouver l'avoir [ICI](https://home.openweathermap.org/users/sign_up)
ETAPE 2 : Dans la section OLED, décomenter la ligen correspondant a votre ecran (UNE A LA FOIS SINON CELA NE MARCHERA PAS !!) 
ETAPE 3 :  Placer tous les fichier dans un dossier Station_meteo et ensuite lance arduino IDE et selectionner Station_meteo.ino et televerser le vers votre ESP8266

INFO : Une fois allume, l'esp fais 1 cycle de toutes les infos. l'ecran s'eteins ensuite et s'allume pendant 15 secondes avec l'heure toutes les 5 min. Vous pouvez mettre un bouton sur D3/GND. SI le bouton est presser, l'ecran s'allume et demarre son cycle des previsions, puis l'ecran s'eteins et se rallume toutes les 5min pour afficher l'heure.
