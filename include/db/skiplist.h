

#ifndef SKIPLIST_H
#define SKIPLIST_H

#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <ctime>
#include <mutex>
#include <thread>
#include <sstream>

template <typename Key, typename Value>
class SkipNode {
public:
    Key key;
    Value value;
    SkipNode<Key, Value>** next;

    SkipNode(Key k, Value val, int level) : key(k), value(val) {
        next = new SkipNode<Key, Value>*[level + 1];
        for (int i = 0; i <= level; ++i) {
            next[i] = nullptr;
        }
    }

    ~SkipNode() {
        delete[] next;
    }
};

template <typename Key, typename Value>
class SkipList {
private:
    int maxLevel;
    int currentLevel;
    SkipNode<Key, Value>* head;
    std::mutex mutex;

    int randomLevel() {
        int level = 0;
        while (rand() % 2 == 0 && level < maxLevel) {
            level++;
        }
        return level;
    }

     // file operator
    std::ofstream _file_writer;
    std::ifstream _file_reader;

public:
    SkipList(int max_lev) : maxLevel(max_lev), currentLevel(0) {
        head = new SkipNode<Key, Value>(Key(), Value(), maxLevel);
    }

    ~SkipList() {
        delete head;
    }

   

    Value search(Key key) {
        std::lock_guard<std::mutex> lock(mutex);

        SkipNode<Key, Value>* current = head;
        for (int i = currentLevel; i >= 0; --i) {
            while (current->next[i] != nullptr && current->next[i]->key < key) {
                current = current->next[i];
            }
        }
        current = current->next[0];
      

        if (current != nullptr && current->key == key) {
            auto val = current->value;
            return val;
        }

        return nullptr;
    }

    void insert(Key key, Value val) {

       
        int level = randomLevel();

        std::lock_guard<std::mutex> lock(mutex);

        // Search for the key to update
        SkipNode<Key, Value>* current = head;
        for (int i = currentLevel; i >= 0; --i) {
            while (current->next[i] != nullptr && current->next[i]->key < key) {
                current = current->next[i];
            }
        }
        current = current->next[0];
        
        // std::cout<<current->key<<std::endl;
        if (current != nullptr && current->key == key) {
            // Key found, remove and insert with new value
            remove(key);
            insert(key, val);
            return;
        }


        if (level > currentLevel) {
            for (int i = currentLevel + 1; i <= level; ++i) {
                head->next[i] = nullptr;
            }
            currentLevel = level;
        }

        SkipNode<Key, Value>* newNode = new SkipNode<Key, Value>(key, val, level);
        current = head;

        for (int i = currentLevel; i >= 0; --i) {
            while (current->next[i] != nullptr && current->next[i]->key < key) {
                current = current->next[i];
            }
            if (i <= level) {
                newNode->next[i] = current->next[i];
                current->next[i] = newNode;
            }
        }
    }

    bool remove(Key key) {
        std::lock_guard<std::mutex> lock(mutex);

        bool removed = false;
        SkipNode<Key, Value>* current = head;
        SkipNode<Key, Value>* toRemove = nullptr;

        for (int i = currentLevel; i >= 0; --i) {
            while (current->next[i] != nullptr && current->next[i]->key < key) {
                current = current->next[i];
            }
            if (current->next[i] != nullptr && current->next[i]->key == key) {
                toRemove = current->next[i];
                current->next[i] = current->next[i]->next[i];
                removed = true;
            }
        }

        if (removed) {
            delete toRemove;
        }

        return removed;
    }
    
    void printList() {
        std::lock_guard<std::mutex> lock(mutex);

        for (int i = currentLevel; i >= 0; --i) {
            SkipNode<Key, Value>* current = head->next[i];
            std::cout << "Level " << i << ": ";
            while (current != nullptr) {
                std::cout << "(" << current->key << ", " << current->value << ") ";
                current = current->next[i];
            }
            std::cout << "\n";
        }
    }

    void writeToFile(const std::string& filename) {
        std::lock_guard<std::mutex> lock(mutex);
        _file_writer.open(filename);

        SkipNode<Key, Value>* current = head;
    
        for (int i = currentLevel; i >= 0; --i) {
            SkipNode<Key, Value>* current = head->next[i];
          
            while (current != nullptr) {
                _file_writer << current->key << " " << current->value << "\n";
                current = current->next[i];
            }
        }
        
        std::cout<<currentLevel<<std::endl;

        _file_writer.flush();
        _file_writer.close();
    }

    void readFromFile(const std::string& filename) {
        std::lock_guard<std::mutex> lock(mutex);

        _file_reader.open(filename);
      
        // Clear existing data
        delete head;
        head = new SkipNode<Key, Value>(Key(), Value(), maxLevel);

        // Read and insert each key-value pair
        Key key;
        Value value;
        while (_file_reader >> key >> value) {
            insert(key, value);
        }

        _file_reader.close();
    }
};

#endif