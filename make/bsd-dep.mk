DFILES != perl -e 'print join " ", grep s/\.cpp/.d/, <*.cpp>, <commands/*.cpp>, <modes/*.cpp>, <modules/*.cpp>, <modules/m_spanningtree/*.cpp>'
DFILES += socketengines/$(SOCKETENGINE).d threadengines/threadengine_pthread.d

alldep: $(DFILES)

.SUFFIXES: .d .cpp

.cpp.d:
	@../make/calcdep.pl $<
	@echo -n .
