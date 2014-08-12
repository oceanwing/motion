TARGET:=test
CC:=gcc
FILES=`ls *.c`

all: $(TARGET)

$(TARGET): *.o
	$(CC) -o $@ $^

clean:
	-rm $(TARGET) *.o

define compile_c_file
@for file in $(FILES); do \
( echo "$(CC) -c $$file" && $(CC) -c $$file ) \
done;
endef

%.o: %.c
	$(call compile_c_file)
