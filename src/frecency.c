#include<stdio.h>
#include<string.h>
#include<unistd.h>
#include<limits.h>

#include "frecency.h"

#define LOG_FILE "hop.log"
#define MAX_ENTRIES 1000

struct frecency_entry{
    char path[PATH_MAX];
    long long frequency;
    long long last_visit;
};

struct frecency_entry entries[MAX_ENTRIES];
int entry_count = 0;
long long visit_counter = 0;

void load_frecency(){
    FILE *fp=fopen(LOG_FILE,"r");
    if(fp==NULL){
        return;
    }

    char line[PATH_MAX + 100];
    while(fgets(line,sizeof(line),fp)!=NULL){
        if(entry_count>=MAX_ENTRIES){
            break;
        }

        long long frequency;
        long long last_visit;
        char path[PATH_MAX];
        if(sscanf(line,
                   "%lld %lld %[^\n]",
                   &frequency,
                   &last_visit,
                   path)==3){

            strcpy(entries[entry_count].path,path);

            entries[entry_count].frequency=frequency;

            entries[entry_count].last_visit=last_visit;

            if(last_visit>visit_counter){
                visit_counter=last_visit;
            }
            entry_count++;
        }
    }
    fclose(fp);
}
void save_frecency(){
    FILE *fp=fopen(LOG_FILE,"w");
    if(fp==NULL){
        return;
    }
    for(int i=0;i<entry_count;i++){
        fprintf(fp,
                "%lld %lld %s\n",
                entries[i].frequency,
                entries[i].last_visit,
                entries[i].path);
    }
    fclose(fp);
}


void init_frecency(){
    load_frecency();
}

void record_visit(char *path){
    visit_counter++;

    /* Directory already exists */
    for(int i=0;i<entry_count;i++){

        if(strcmp(entries[i].path,path)==0){
            entries[i].frequency++;

            entries[i].last_visit=visit_counter;
            save_frecency();
            return;
        }
    }

    /* No space for a new entry */
    if(entry_count>=MAX_ENTRIES) {
        return;
    }

    /* Add new directory */
    strcpy(entries[entry_count].path,path);

    entries[entry_count].frequency=1;

    entries[entry_count].last_visit=visit_counter;

    entry_count++;

    save_frecency();
}


/* Find best frecency match */
int find_frecency_match(char *name,char *result){
    long long best_score=-1;

    int best_index=-1;

    for(int i=0;i<entry_count;i++){
        /*
         * The requested name must occur
         * somewhere in the saved path.
         */
        if(strstr(entries[i].path,name)==NULL){
            continue;
        }

        /*
         * Skip paths that no longer exist.
         */
        if (access(entries[i].path,F_OK)!=0){
            continue;
        }

        /*
         * Frequency + recency.
         */
        long long score =entries[i].frequency * 1000 + entries[i].last_visit;
        if (best_index==-1||
            score>best_score){
            best_score=score;
            best_index=i;
        }
    }
    if(best_index==-1){
        return 0;
    }

    strcpy(result,entries[best_index].path);

    return 1;
}