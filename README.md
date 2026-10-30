# Mini-shell

Shell pédagogique en C pour un environnement POSIX (Linux, macOS).
Il exécute des commandes, des pipelines, des redirections `<` et `>` et des
jobs au premier ou à l'arrière-plan.

## Compiler et lancer

```sh
make clean
make
./shell
```

Prérequis : compilateur C, Make et bibliothèque POSIX. Aucun téléchargement
supplémentaire n'est nécessaire. `make clean` supprime les produits de compilation.

## Gestion des jobs

```text
shell> sleep 30 &
[1] 12345
shell> jobs
[1]: PID=12345 (Background): sleep
shell> fg %1
```

- `jobs` affiche les jobs actifs et leur état (`Background` ou `Stopped`).
- `fg 1` ou `fg %1` reprend le job au premier plan et attend sa fin ou son arrêt.
- `bg 1` ou `bg %1` reprend le job à l'arrière-plan et rend immédiatement la main.
- `Ctrl+Z` suspend le job au premier plan ; `Ctrl+C` l'interrompt.
- `kill 1` ou `kill %1` envoie SIGTERM au groupe du job et reprend un job
  suspendu pour permettre sa terminaison.
- `cd [répertoire]` change le répertoire courant ; sans argument, il utilise HOME.
- `quit` ou la fin de l'entrée ferme le shell et envoie SIGHUP puis SIGCONT aux
  groupes encore présents. Un programme qui ignore SIGHUP peut survivre.

Un pipeline entier, par exemple `sleep 30 | cat`, forme **un seul job**.
Les signaux et `fg`/`bg` concernent tous ses processus. Le shell attend que
chaque processus soit terminé ou suspendu avant de reprendre le terminal.
Un programme à l'arrière-plan qui tente de lire le terminal est suspendu par
le système ; `fg` lui redonne accès au terminal.

Les numéros sont des positions réutilisables dans une table de **15 jobs**.
`fg`, `bg` et `kill` exigent un numéro explicite ; les arguments absents,
invalides et les jobs inexistants sont signalés sans arrêter le shell.
Les processus terminés sont récupérés même pendant l'attente d'une saisie.

## Commandes et limites

```text
echo bonjour | cat
cat < entree.txt | wc > sortie.txt
sleep 2&
```

`&` doit terminer une commande ou un pipeline. Les commandes internes (`cd`,
`jobs`, `fg`, `bg`, `kill`, `quit`) s'utilisent seules, sans pipe, redirection
ni `&` ; ces combinaisons sont refusées avant de lancer des processus.
Le parseur pédagogique ne gère ni guillemets, ni variables, ni jokers, ni `;`,
ni opérateurs `&&`/`||`. Le nom affiché par `jobs` est celui de la première
commande du pipeline, pas sa ligne complète.

## Vérification

```sh
make test
```

Les tests utilisent Python 3 et sa bibliothèque standard. Ils ouvrent de vrais
pseudo-terminaux et vérifient notamment les identifiants invalides, la reprise
`fg`/`bg`, Ctrl+C/Ctrl+Z, les pipelines, l'accès au terminal, ses modes, les
processus rapides, la récupération des enfants, la saturation de la table et
les redirections. Linux permet aussi de vérifier via `/proc` la récupération
des enfants lorsque le shell attend une saisie.

Les scénarios historiques `tests/test*.txt` et `sdriver.pl` sont conservés ;
ils servent de traces pédagogiques et ne constituent pas des assertions
automatisées. `fichiers_fournis/` dans le dépôt GitHub conserve les sources
initiales et n'est pas modifié par cette correction.
