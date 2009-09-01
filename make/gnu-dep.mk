DFILES = $(patsubst %.cpp,%.d,$(wildcard *.cpp))
DFILES += $(patsubst %.cpp,%.d,$(wildcard commands/*.cpp))
DFILES += $(patsubst %.cpp,%.d,$(wildcard modes/*.cpp))
DFILES += $(patsubst %.cpp,%.d,$(wildcard modules/*.cpp))
DFILES += socketengines/$(SOCKETENGINE).d threadengines/threadengine_pthread.d

all: $(DFILES)

%.d: %.cpp
	@../make/calcdep.pl $<
	@echo -n .
