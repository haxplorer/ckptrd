# include "lam-ssi-ckptrd-includes.h"

void make_mem();

int main()
{
    lam_cr_dbootmesg dbootmesg;
    dbootmesg.node_count = 5;
    dbootmesg.node_ips = malloc(5 * sizeof(char *));
    dbootmesg.node_ips[0] = strdup("10.10.10.101");
    dbootmesg.node_ips[1] = strdup("10.10.10.102");
    dbootmesg.node_ips[2] = strdup("10.10.10.103");
    dbootmesg.node_ips[3] = strdup("10.10.10.104");
    dbootmesg.node_ips[4] = strdup("10.10.10.105");

    printf("Making ..\n");
    make_mem();

    printf("Writing ..%d\n", sizeof(char *));
    lam_ssi_ckptrd_create_node_ips(dbootmesg);

    printf("Reading ..\n");
    char **node_ips = lam_ssi_ckptrd_get_node_ips();

    int i;
    for(i=0;i<5;i++)
    {
        printf("%s\n", node_ips[i]);
    }
}
