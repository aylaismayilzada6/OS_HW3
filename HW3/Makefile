CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2
TARGET   := fssim
SRCDIR   := src

SRCS := $(SRCDIR)/main.cpp \
        $(SRCDIR)/disk.cpp \
        $(SRCDIR)/journal.cpp \
        $(SRCDIR)/contiguous_fs.cpp \
        $(SRCDIR)/fat_fs.cpp \
        $(SRCDIR)/inode_fs.cpp \
        $(SRCDIR)/directory.cpp

OBJS := $(SRCS:.cpp=.o)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(SRCDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

run: all
	./$(TARGET)

clean:
	rm -f $(SRCDIR)/*.o $(TARGET)
