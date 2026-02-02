# Clavier mécanique programmable – Projet EEL8-PROJ1 [CLA]

## 📌 Présentation du projet

Ce dépôt contient le travail réalisé dans le cadre du **projet thématique EEL8-PROJ1 (2025–2026)** à l’ENSEIRB-MATMECA.  
L’objectif du projet est de **concevoir et réaliser un clavier mécanique programmable** basé sur un **microcontrôleur**, communiquant avec un ordinateur via le standard **USB HID**.

Contrairement aux solutions existantes « clé en main » (ex. QMK), ce projet privilégie le **développement d’un firmware personnalisé**, afin de maîtriser les aspects bas niveau du système embarqué.

---

## 🎯 Objectifs

- Concevoir un **clavier mécanique compact ou minimaliste**
- Implémenter un **scan matriciel des touches**
- Gérer le **n-key rollover** et l’**anti-ghosting**
- Développer un **firmware bas niveau** sur microcontrôleur
- Communiquer avec un ordinateur via **USB HID**
- Ajouter une **interface utilisateur** :
  - LEDs sous les touches
  - Gestion de profils
  - Écran OLED pour l’affichage du profil actif

---

## 🧠 Fonctionnalités principales

- Lecture matricielle des touches
- Détection fiable des appuis multiples
- Gestion de plusieurs profils clavier
- Éclairage LED configurable selon le profil
- Affichage des informations sur écran OLED
- Communication USB HID avec l’ordinateur

---

## 🛠️ Technologies utilisées

- **Microcontrôleur** : Raspberry Pi Pico (RP2040) ou équivalent [git ici](https://github.com/raspberrypi/pico-sdk)
- **Langage** : C / C++ (firmware embarqué)
- **Communication** : USB HID
- **Conception électronique** : PCB personnalisé
- **Interface utilisateur** :
  - LEDs adressables
  - Écran OLED

---

## 🧩 Organisation du projet

```text
├── firmware/        # Code source du firmware
├── hardware/        # Schémas électroniques et PCB
├── docs/            # Documentation technique
├── images/          # Illustrations et photos du projet
└── README.md
