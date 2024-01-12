all:
	gcc disdrive.c -o disdrive.so -fPIC -pthread -lcurl -shared
