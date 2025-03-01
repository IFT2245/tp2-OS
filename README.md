echo "3\n1\n6\n" | valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --trace-children=yes --child-silent-after-fork=no ./main 2&> ../../output-valgrind.txt
