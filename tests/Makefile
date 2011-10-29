FSTEST_OBJS = fstest.o

fstest: $(FSTEST_OBJS) $(DEPENDS)
	$(CC) -o fstest $(FSTEST_OBJS) $(LDFLAGS) -lrt -lz -lcrypto

clean:
	@rm -f fstest $(FSTEST_OBJS)
