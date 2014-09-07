nodemon -w . -e "c h" -x 'sh -c' 'cc -std=c99 -Wall -Werror parsing.c mpc.c -ledit -o parsing && growlnotify -m OK || growlnotify -m FAIL'
