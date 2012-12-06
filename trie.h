#include <list>
#include <string>
using namespace std;
struct cidrprefix{
    uint32_t prefix;
    char size;
};
struct trienode {
    struct trienode* onechild;
    struct trienode* zerochild;
    string value;
};
class trie {
    public:
        trie();
        void insert(struct cidrprefix prefix, string interface);
        string search(uint32_t ip);
    private:
        struct trienode listhead;
        list<struct trienode> nodes;
};

/*trienode::trienode(): onechild(NULL), zerochild(NULL), value(""){
}*/

trie::trie(): listhead(), nodes(1, listhead) {
}

void trie::insert(struct cidrprefix prefix, string interface){
    struct trienode *currentnode = &(this->listhead);
    struct trienode *nextnode;
    for(int i = 0; i<prefix.size; i++){
        uint32_t mask = 0x80000000>>i;
        if(mask&prefix.prefix){
            if(currentnode->onechild==NULL){
                //TODO make sure this memory does not leak
                struct trienode *newnode = new trienode();
                (this->nodes).push_back(*newnode);
                nextnode = &(this->nodes).back();
                currentnode->onechild = nextnode;
            } else {
                nextnode = currentnode->onechild;
            }
        } else {
            if(currentnode->zerochild==NULL){
                //TODO make sure this memory does not leak
                struct trienode *newnode = new trienode();
                (this->nodes).push_back(*newnode);
                nextnode = &(this->nodes).back();
                currentnode->zerochild = nextnode;
            } else {
                nextnode = currentnode->zerochild;
            }
        }
        currentnode = nextnode;
    }
    currentnode->value = interface;
}

string trie::search(uint32_t ip) {
    string lastval = "";
    struct trienode *currentnode = &(this->listhead);
    int i = 0;
    while((currentnode!=NULL)&&i<32){
        uint32_t mask = 0x80000000>>i;
        i++;
        if(!(currentnode->value).compare("")){
            lastval=currentnode->value;
        }
        if(mask&ip){
            currentnode=currentnode->onechild;
        }else{
            currentnode=currentnode->zerochild;
        }
    }
    return lastval;
}
