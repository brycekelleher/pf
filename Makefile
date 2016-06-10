OBJECTS	= glsim.o
CXX = clang

#ifeq ($(APPLE),1)
CXXFLAGS += -g -I/usr/X11R6/include -DGL_GLEXT_PROTOTYPES
LDFLAGS = -L/usr/X11R6/lib
LDLIBS  = -lGL -lglut -lm
#endif

glsim: $(OBJECTS)

clean:
	rm -rf glsim $(OBJECTS)
