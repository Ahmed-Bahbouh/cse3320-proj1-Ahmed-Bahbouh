#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
    int i = 0;

	sleep(500);
    
    for (i = 1; i < argc; i++)
    {
        printf(1, argv[i]);
        printf(1, " ");
    }

    printf(1, "\n");
    
    exit();
}
