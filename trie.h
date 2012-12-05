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

trie::trie(): listhead(), nodes(1, listhead) {
}

void trie::insert(struct cidrprefix prefix, string interface){
   //insert
}

string trie::search(uint32_t ip) {
   return "";
}
