# ArmibuleFish

## 👋 Présentation du projet

Il s'agit d'un **bot d'échecs** que j'ai programmé par moi même en utilisant des conseils et techniques figurant sur le site [chessprogramming](https://chessprogramming.org) et le server discord **Engine Programming**

Il en est à la **troisième itération**, la première version étant en Python et la seconde aussi en C++.

Sachant qu'il s'agit de l'un de mes premiers projets (plus ou moins) aboutis en C++, certaines bonnes pratiques ne sont pas respectées (comme l'absences de fichiers headers par exemple).

Il y a aussi pas mal de tests dans le code.

### 🧩 Features implémentées

Voici les techniques que j'ai (j'espère correctement) implémentées :

 - L'algorithme **MinMax** avec **Alpha-Beta Pruning** + **Principal Variation Search**

 - Utilisation de **magic bitBoards** et **bitscans** pour la génération des coups

 - Une **Table de transposition** contenant les coups précédemment cherchés, stocké grace à un extrait du **Hash Zobrist** du noeud

 - Tri des coups :
   - Meilleur coup de la **Table de transposition**
   - Captures avec **SEE** > 0
   - Coups **killers**
   - Coups triés par **historique** / captures **SEE** < 0

 - Le **Null Move Pruning** et **Reverse Futility Pruning** permettent d'éviter de chercher les noeuds "trop bons"

 - Les **Late Move Reductions** réduisant les recherches sur les coups les plus mauvais

 - La **Quiescence Search** limitant l'effet d'horizon de la recherche

 - Une **Fonction d'évaluation statique** basée sur sur un réseau neuronal / **NNUE** entrainée avec mon trainer sur une base de données lichess
   - Architecture : Input **2×786** -> **128** -> **30** Output buckets 
  
 - Un **historique de correction de l'évaluation statique** basé sur la **structure des pions** et le **matériel**

 - De **l'Approfondissement Itératif** prenant en compte les recherches précédentes pour les accélérer

 - Un mode **UCI** ***bancal*** sans option de pondération (toujours activée), ni te taille de hash


### 🔧 Performances

Sur mon ordinateur, avec un processeur assez ancien (`Intel Core i7-6700HQ 2.60GHz`), une recherche de **4s**-**10s** donne une **profondeur maximale** entre **15** et **22** plis, dépendant de la complexité de la position. 

Quant au **niveau** atteint par ce bot, situe à **2819 +/- 35 Elo** à 8.0s+0.08s (d'après des tests contre Stash 21.2).

### 📈 Améliorations possibles

 - Implémenter de nouvelles techniques comme l'**aspiration window**
 - Plus utilise la SEE
 - Améliorer les **constantes** de recherche (LMR, NMP, ...) peut-être avec l'algorithme SPSA ?
 - Améliorer le réseau neuronal ? (King input buckets, + de données...)
 - Améliorer la vitesse d'inférence ?

Objectif : battre le bot chess.com de Magnus Carlsen !

## 📥 Installation des librairies

Sur windows, ustilisez de préférence la toolchain **msys** avec **mingw64**.

-  **SDL** -  La librairie graphique pour la gestion de la fenêtre.  
   > `pacman -S mingw-w64-x86_64-SDL2`

- **SDL_ttf** - La librairie de rendu de texte et de gestion de police d'écriture.  
  > `pacman -S mingw-w64-x86_64-SDL2_ttf`

- **SDL_gfx** - La librairie étendant les fonctions graphiques de SDL.  
  > `pacman -S mingw-w64-x86_64-SDL2_gfx`

## ⚙ Compilation

Pour **compiler** le projet, après l'installation des librairies, choisissez des lignes de comandes parmi celles des fichiers en `.sh`.

**Windows** et **Linux** sont maintenant tous deux supportés !

## 🔑 Utilisation

### Mode normal

Il s'agit d'un plateau de jeu sur lequel vous pouvez déplacer les pièces et faire jouer le bot.

- Pour faire **jouer le bot** d'un coup appuyez sur la touche `B`.

- Vous pouvez activer/désactiver la **prévision des coups** par le bot en appuyant sur `P`. La Variation Principale est affichée à l'aide de flèches

- Vous pouvez **annuler un coup** en appuyant sur `U`

- Dans le menu paramètres vous pouvez afficher/cacher le nombre de noeuds recherchés lors du dernier coup. (Ce menu est plutôt vide mais d'autres actions y seront ajoutées)

 - **Arguments de la ligne de commande**
   - **--help** : Affiche les arguments disponibles
   - **--white** : Le bot joue pour les blancs à chaque tour automatiquement
   - **--black** : Le bot joue pour les noirs à chaque tour automatiquement
   - **-fen "\<FEN string>"** : Charge une position d'après une notation FEN
   - **-time \<TargetTime> \<MaxTime>** : Définit le temps de recherche du bot en millisecondes. Après MaxTime millisecondes, la recherche est arrêtées brusquement.
