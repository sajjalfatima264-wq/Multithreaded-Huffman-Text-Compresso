#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CHARS 256 

typedef struct Node {
    unsigned char ch;
    long freq;
    struct Node *left, *right;
} Node;

typedef struct {
    Node* data[MAX_CHARS];
    int size;
} MinHeap;

typedef struct {
    unsigned char ch;
    long freq;        // Added to store for final output
    char *code;
} CodeEntry;

/* Heap functions */
void swap(Node **a, Node **b){ Node *t=*a; *a=*b; *b=t; }

void heapify(MinHeap *h, int i){
    int smallest=i;
    int l=2*i+1, r=2*i+2;
    if(l<h->size && h->data[l]->freq < h->data[smallest]->freq) smallest=l;
    if(r<h->size && h->data[r]->freq < h->data[smallest]->freq) smallest=r;
    if(smallest!=i){ swap(&h->data[i], &h->data[smallest]); heapify(h,smallest);}
}

Node* extract_min(MinHeap *h){
    Node* temp=h->data[0];
    h->data[0]=h->data[h->size-1];
    h->size--;
    heapify(h,0);
    return temp;
}

void insert_heap(MinHeap *h, Node *n){
    h->size++;
    int i=h->size-1;
    h->data[i]=n;
    while(i && h->data[(i-1)/2]->freq > h->data[i]->freq){
        swap(&h->data[i], &h->data[(i-1)/2]);
        i=(i-1)/2;
    }
}

void assign_codes(Node *root, char *code, int depth, char *codes[MAX_CHARS]){
    if(!root) return;
    if(!root->left && !root->right){
        code[depth]='\0';
        codes[(unsigned char)root->ch]=strdup(code);
        return;
    }
    if(root->left){
        code[depth]='0';
        assign_codes(root->left, code, depth+1, codes);
    }
    if(root->right){
        code[depth]='1';
        assign_codes(root->right, code, depth+1, codes);
    }
}

/* Comparison function: Sort by Code Length, then Frequency (desc), then ASCII */
int compare_entries(const void *a, const void *b){
    CodeEntry *ea = (CodeEntry*)a;
    CodeEntry *eb = (CodeEntry*)b;
    
    // 1. Shortest code length first
    int len_a = strlen(ea->code);
    int len_b = strlen(eb->code);
    if(len_a != len_b) return len_a - len_b;
    
    // 2. Higher frequency first (if lengths are equal)
    if(eb->freq != ea->freq) return (eb->freq > ea->freq) ? 1 : -1;
    
    // 3. ASCII order tie-breaker
    return (int)ea->ch - (int)eb->ch;
}

int main(int argc, char *argv[]){
    if(argc<2){ 
        printf("Usage: %s char_frequency.txt\n", argv[0]); 
        return 1; 
    }

    FILE *fp=fopen(argv[1],"r");
    if(!fp){ perror("Cannot open frequency file"); return 1; }

    long freqs[MAX_CHARS]={0};
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    rewind(fp);

    char *buffer = (char*)malloc(file_size + 1);
    fread(buffer, 1, file_size, fp);
    buffer[file_size] = '\0';
    fclose(fp);

    // Split entries by semicolon
    char *entry = strtok(buffer, ";");
    while(entry){
        unsigned char ch;
        long freq;

        // Handle special characters
        if(strncmp(entry, "\\n,", 3) == 0) { ch = '\n'; sscanf(entry+3, "%ld", &freq); }
        else if(strncmp(entry, "\\r,", 3) == 0) { ch = '\r'; sscanf(entry+3, "%ld", &freq); }
        else if(strncmp(entry, "' ',", 4) == 0) { ch = ' '; sscanf(entry+4, "%ld", &freq); }
        else { 
            ch = (unsigned char)entry[0]; 
            sscanf(entry + 2, "%ld", &freq); 
        }

        freqs[ch] = freq;
        entry = strtok(NULL, ";");
    }
    free(buffer);

    // Build MinHeap
    MinHeap heap = {.size = 0};
    for(int i = 0; i < MAX_CHARS; i++){
        if(freqs[i] > 0){
            Node *n = (Node*)malloc(sizeof(Node));
            n->ch = (unsigned char)i; 
            n->freq = freqs[i]; 
            n->left = n->right = NULL;
            insert_heap(&heap, n);
        }
    }

    if(heap.size == 0) return 0;

    // Build Huffman tree
    while(heap.size > 1){
        Node *x = extract_min(&heap);
        Node *y = extract_min(&heap);
        Node *z = (Node*)malloc(sizeof(Node));
        z->ch = 0; z->freq = x->freq + y->freq; z->left = x; z->right = y;
        insert_heap(&heap, z);
    }

    Node *root = extract_min(&heap);
    char *codes[MAX_CHARS] = {0};
    char code_buffer[MAX_CHARS];
    assign_codes(root, code_buffer, 0, codes);

    CodeEntry entries[MAX_CHARS];
    int n_entries = 0;
    for(int i = 0; i < MAX_CHARS; i++){
        if(codes[i]){
            entries[n_entries].ch = (unsigned char)i;
            entries[n_entries].freq = freqs[i];
            entries[n_entries].code = codes[i];
            n_entries++;
        }
    }

    // Sort and output
    qsort(entries, n_entries, sizeof(CodeEntry), compare_entries);

    FILE *out = fopen("encoding.txt", "w");
    fprintf(out, "Char | Frequency | Huffman Code\n");
    fprintf(out, "-------------------------------\n");
    for(int i = 0; i < n_entries; i++){
        unsigned char c = entries[i].ch;
        char label[10];

        if(c == '\n') strcpy(label, "\\n");
        else if(c == '\r') strcpy(label, "\\r");
        else if(c == ' ') strcpy(label, "' '");
        else { label[0] = c; label[1] = '\0'; }

        fprintf(out, "%-5s | %-9ld | %s\n", label, entries[i].freq, entries[i].code);
    }
    fclose(out);

    for(int i = 0; i < MAX_CHARS; i++) if(codes[i]) free(codes[i]);
    printf("Sorted Extended ASCII Encoding complete: encoding.txt generated.\n");

    return 0;
}

