#ifndef SPLAYTREESET_H_INCLUDED
#define SPLAYTREESET_H_INCLUDED
#include<vector>
#include<memory>
using namespace std;
#define lson (0)
#define rson (1)
template<typename T>
class TreeNode{
public:
    typedef int size_type;
    template<typename T1>
    friend class SplayTreeSet;
    TreeNode(T _value,shared_ptr<TreeNode<T>> _father = nullptr){
        value = _value;
        father = _father;
        son[0] = son[1] = nullptr;
    }
private:
    size_type cnt;
    T value;
    shared_ptr<TreeNode<T>> son[2];
    weak_ptr<TreeNode<T>> father;
};
template<typename T1>
class SplayTreeSet{
public:
    typedef int size_type;
    typedef T1 value_type;
    typedef TreeNode<T1> Node;
    SplayTreeSet(){
        Size = 0;
        root = nullptr;
    }
    void clear(){
        Size = 0;
        root = nullptr;
    }
    void insert(value_type val){
        if(root == nullptr){
            Size++;
            root = make_shared<Node>(Node(val));
        }
        shared_ptr<Node> t = root;
        while(t){
            if(t->value == val){
                splay(t,nullptr);
                return;
            }
            bool which = (val > t->value);
            if(t->son[which] == nullptr){
                t->son[which] = make_shared<Node>(val,t);
                Size++;
            }
            t = t->son[which];
        }
    }
    size_type size(){
        return Size;
    }
    size_type count(value_type val){
        shared_ptr<Node> t = root;
        while(t){
            if(t->value == val){
                splay(t,nullptr);
                return t->cnt;
            }
            t = t->son[val > t->value];
        }
        return 0;
    }
    bool erase(value_type val){
        shared_ptr<Node> t = where_value(val);
        if(t == nullptr)
            return false;
        size--;
        splay(t,nullptr);
        if(t->son[lson] == nullptr){
            root = t->son[rson];
            if(root)
                root->father = nullptr;
        }
        else{
            shared_ptr<Node> p = t->son[lson];
            while(p->son[rson] != nullptr)
                p = p->son[rson];
            splay(p,t);
            root = p;
            root->father = nullptr;
            p->son[rson] = t->son[rson];
            if(p->son[rson])
                p->son[rson]->father = p;
        }
        return true;
    }
private:
    shared_ptr<Node> root;
    size_type Size;
    bool which_son(shared_ptr<Node> f,shared_ptr<Node> s){
        return f->son[rson] == s;
    }
    void rotate(shared_ptr<Node> t){
        shared_ptr<Node> f = t->father.lock();
        shared_ptr<Node> g = f->father.lock();
        bool which = which_son(f,t);
        f->son[which] = t->son[!which];
        t->son[!which] = f;
        f->father = t;
        t->father = g;
        if(f->son[which])
            f->son[which]->father = f;
        if(g)
            g->son[which_son(g,f)] = t;
        else
            root = t;
    }
    void splay(shared_ptr<Node> t,shared_ptr<Node> p){
        while(t->father.lock() != p){
            shared_ptr<Node> f = t->father.lock();
            shared_ptr<Node> g = f->father.lock();
            if(g == p)
                rotate(t);
            else{
                if(which_son(g,f) == which_son(f,t)){
                    rotate(f);rotate(t);
                }
                else{
                    rotate(t);rotate(t);
                }
            }
        }
    }
    shared_ptr<Node> where_value(value_type _value){
        shared_ptr<Node> t = root;
        while(t){
            if(t->value == _value)
                return t;
            t = t->son[_value > t->value];
        }
        return nullptr;
    }
};


#endif // SPLAYTREESET_H_INCLUDED
