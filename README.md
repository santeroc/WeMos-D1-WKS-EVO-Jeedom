# WeMos-D1-WKS-EVO-Jeedom
Module permettant de collecter les informations d'un onduleur WKS-EVO pour les récupérer dans une plateforme domotique Jeedom.

## Description détaillée

Ce projet a pour objectif de mettre en œuvre un module permettant de collecter les informations en provenance d’un onduleur solaire hybride WKS, de contrôler à l’aide de deux sondes thermiques la température haute (côté onduleur), la température basse (côté batteries) et de démarrer un système de ventilation si nécessaire. Ce module maintient également la date et l’heure à jour en face du changement d'heure d'été et d'hiver dans l'onduleur (qui n'est pas connecté au réseau informatique). La carte ARCELI WeMos D1 R2, à la base de ce module, embarque une architecture ESP8266 et une carte WIFI intégrée.

## Recommandation
Il est vivement recommandé d'avoir de bonnes connaissances en électricité générale avant de réaliser ou de manipuler tout câblage électrique.
Ceci afin d'appréhender au minimum les risques d'électrocution.

## Structure du projet
- **Documentation\\** : Documentation du projet à lire avant de commencer
	- ``WeMos-D1-WKS-EVO-Jeedom.pdf`` : Document de base nécessaire pour fabriquer le projet et utiliser le code source ci-dessous.
	- ``Plan_De_Cablage_Electrique.pdf`` : Plan de câblage électrique de la solution.
	- ``Plan_De_Cablage_Electrique_Element_Positon_Plan.pdf`` / ``Plan_De_Cablage_Electrique_Element_Positon_Plan.html`` : Composants du plan de câblage avec position sur plan.
	- ``Manuel-WKS-EVO-FR-2021-2-2 - Avec Mes Paramétres.pdf`` : Manuel du WKS-EVO avec mes paramètres et mes commentaires en couleurs.
- **Facade-3D\\**     : Façade imprimable en 3D réalisée avec FreeCAD 0.20
	- ``Facade-v3.FCStd`` : Projet de façade réalisé avec FreeCAD.
	-  ``Facade-v3-Body (Meshed).stl`` : Fichier Mesh de la façade 3D à transmettre pour l'impression 3D.
- **Source\\**        : Code source du projet 
	-  ``WKS_ip_Claude_LCD2004_Et_Temp_H_B_R_T_Reboot_STOndul.ino`` : Code source Arduino à adapter selon la documentaiton ci-dessus.
	
## Version
**Dernière version stable :** 7.8

## Auteur
* **Claude Santero** _alias_ [@santeroc](https://github.com/santeroc)

## Licence

Ce projet est sous licence ``GPLv3`` - voir le fichier [LICENSE.md](LICENSE.md) pour plus d'informations.
