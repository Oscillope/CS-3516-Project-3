#include <list>
#include <string>
using namespace std;
struct trienode {
    struct trienode* onechild;
    struct trienode* zerochild;
    string value;
};
class trie {
    public:
        trie();
        void insert(string prefix, string interface);
        string search(string ip);
    private:
        struct trienode listhead;
        list<struct trienode> nodes;
};

trie::trie(): listhead(), nodes(1, listhead) {
}

void trie::insert(string prefix, string interface){
   //insert
}

string trie::search(string ip) {
   return "";
}
