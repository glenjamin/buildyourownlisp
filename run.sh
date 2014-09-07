nodemon -w . -e "c h" -x 'sh -c' 'cc -std=c99 -Wall -Werror glenisp.c mpc.c -ledit -o glenisp && growlnotify -m OK || growlnotify -m FAIL'
