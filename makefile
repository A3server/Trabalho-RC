
OBJS=	funcs.h main.c funcs.c 
OBJSclient = client.c
OBJSclientadmin = admin.c

GCC=	gcc

PROG= 	news_server
PROGC = news_client
PROGCAD = news_admin



all:	${PROG} ${PROGC} ${PROGCAD}


${PROG}:	${OBJS}
			${GCC} -pthread -o ${PROG} ${OBJS}

${PROGC}:	${OBJSclient}
			${GCC} -o ${PROGC} ${OBJSclient}

${PROGCAD}: ${OBJSclientadmin}
			${GCC} -o ${PROGCAD} ${OBJSclientadmin} 



.c.o:
			${GCC} -c $< -c -o $@

make: 	${OBJS}
	${GCC} ${OBJS} -O ${PROG}
	${GCC} ${OBJSclient} -O ${PROGC}
	${GCC} ${OBJSclientadmin} -O ${PROGCAD}
	