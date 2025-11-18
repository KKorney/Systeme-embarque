⚙️ Système Embarqué d'Acquisition de Données (Arduino)
Ce projet est un système d'acquisition de données autonome et portable, basé sur une plateforme Arduino. Il est conçu pour collecter des données environnementales en temps réel, les stocker de manière fiable, et offrir plusieurs modes de fonctionnement contrôlables par l'utilisateur.

✨ Fonctionnalités Principales
Le système embarqué intègre une gestion complète de l'acquisition, du stockage et de l'interface utilisateur :

Acquisition Multi-capteurs : Lecture de données de température, humidité, pression (via BME280), et luminosité (via un photo-capteur analogique).

Horodatage Précis (RTC) : Utilisation d'un module RTC (DS1307) pour horodater chaque point de données.

Stockage Fiable : Enregistrement des données sur une carte SD. Les données sont stockées dans des fichiers nommés selon la date (JJMMYY_rev.TXT).

Gestion des Modes Opérationnels : Possibilité de basculer entre quatre modes distincts (Standard, Configuration, Maintenance, Économique) via deux boutons-poussoirs.

Interface Visuelle : Utilisation d'une LED RGB pour indiquer le mode de fonctionnement actuel et signaler les erreurs (RTC, SD, Capteurs, etc.).
