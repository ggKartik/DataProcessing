#include "client.h"
#define PORT 3000

int main(){
        Client client;
        client.start(PORT);
        return 0;
}