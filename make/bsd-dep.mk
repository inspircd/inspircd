DFILES != perl -e 'print join " ", <*.cpp>, <commands/*.cpp>, <modes/*.cpp>, <modules/*.cpp>, <modules/m_spanningtree/*.cpp>'
DFILES += socketengines/$(SOCKETENGINE).cpp threadengines/threadengine_pthread.cpp

alldep:
	../make/calcdep.pl $(DFILES)
