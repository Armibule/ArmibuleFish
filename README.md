# Armibule Fish

## 👋 Présentation du projet

Il s'agit d'un **bot d'échecs** que j'ai programmé par moi même en utilisant des conseils et techinques figurant sur le site https://chessprogramming.org. 

Il en est la **troisième itération**, la première version étant en Python et la seconde aussi en C++.

Sachant qu'il s'agit de l'un de mes premiers projets (plus ou moins) aboutis en C++, certaines bonnes pratiques ne sont probablement pas respectées (comme l'absences de fichiers headers par exemple).

### 🧩 Features implémentées

Voici les techniques que j'ai (j'espère correctement) implémentées :

 - L'algorithme **MinMax** avec **Alpha-Beta Pruning**

 - Une **Table de transposition** contenant les coups précédemment cherchés, stocké grace à un extrait du **Hash Zobrist** du noeud

 - Le tri des coups avec un **Move Ordering** sur l'évaluation des noeuds, mais aussi sur la **Table de transposition**

 - Le **Null Move Pruning** permettant d'éviter de chercher les mauvais noeuds

 - Les **Late Move Reductions** réduisant les recherches sur les coups les plus improbables

 - La **Quiescence Search** limitant l'effet d'horizon de la recherche

 - Une **Fonction d'évaluation** basée sur le **matériel**, la **position** des pièces, leur **mobilité** et **sécurité**, la **structure** des pions et d'autres éléments plus spécifiques à chaque pièces

 - De **l'Approfondissement Itératif** prenant en compte les recherches précédentes pour les accélérer

### 🔧 Performances

Sur mon ordinateur, avec un processeur assez ancien (`Intel Core i7-6700HQ 2.60GHz`), la recherche prend **1s**-**10s** avec une profondeur maximale de **8** ou (rarement) **9** plis, dépendant de la complexité de la position. 

Quant au **niveau** atteint par ce bot, il pourrait se situer entre **2000** et **2200 elo**, en se basant sur les bots chess.com qu'il peut battre, mais je ne l'ai pas encore rigoureusement testé.

### 📈 Améliorations possibles

 - Pour l'instant ce bot ne sait pas encore faire la prise **en-passant**
 - Implémenter de nouvelles techniques comme l'**aspiration window**
 - Améliorer les **constantes** choisies pour l'**évaluation** car elles sont pour l'instant assez arbitraires
 - Pour ce faire, améliorer le code de test pour comparer différentes versions du bot
 - Respecter les standards et utiliser les fichiers headers :,)

## 📥 Installation des librairies

Sur windows, ustilisez de préférence la toolchain **msys** avec **mingw64**.

-  **SDL** -  La librairie graphique pour la gestion de la fenêtre.  
   > `pacman -S mingw-w64-x86_64-SDL2`

- **SDL_ttf** - La librairie de rendu de texte et de gestion de police d'écriture.  
  > `pacman -S mingw-w64-x86_64-SDL2_ttf`

- **SDL_gfx** - La librairie étendant les fonctions graphiques de SDL.  
  > `pacman -S mingw-w64-x86_64-SDL2_gfx`

## ⚙ Compilation

Pour **compiler** le projet, après l'installation des librairies, utilisez les lignes de comandes situées dans les fichiers en `.bat`.

Notez que pour l'instant, **seul Windows** est supporté.

## 🔑 Utilisation

Il s'agit d'un plateau de jeu sur lequel vous pouvez déplacer les pièces.

- Pour faire **jouer le bot** d'un coup appuyez sur la touche `B`.

- Vous pouvez activer/désactiver la **prévision des coups** par le bot en appuyant sur `P`. La Variation Principale est affichée à l'aide de flèches

- Vous pouvez **annuler un coup** en appuyant sur `U`

- Dans le menu paramètres vous pouvez afficher/cacher le nombre de noeuds recherchés lors du dernier coup. (Ce menu est plutôt vide mais d'autres actions y seront ajoutées)

- Pour changer autre chose il faut malheureusement **modifier le code**
  - Pour changer la profondeur, modifiez `NORMAL_DEPTH` dans `botConstants.cpp`
  - Pour changer le temps moyen donné au bot par coup, modifiez `TARGET_BOT_TIME` dans `botConstants.cpp`
  - Pour assigner une couleur pour laquelle le bot jouera automatiquement, définissez `botPlaysBlack` ou `botPlaysBlack` à `true` dans `main.cpp`
