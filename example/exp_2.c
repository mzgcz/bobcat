#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <seqfile.h>

int main(int argc, char *argv[])
{
    int8_t ret;
    uint16_t i;
    struct seqfile_info seq_info;
    struct smlfile_info sml_info;
    
    memset(&seq_info, 0, sizeof(seq_info));
    strcpy(seq_info.pdir, "/tmp");
    strcpy(seq_info.fname, "exp_seq_2");
    seq_info.crt_tag = CREATE_WHEN_NONENTITY;
    ret = open_seqfile(&seq_info);
    if (OPERATE_OK != ret) {
        return ret;
    }
    
    for (i=0; i<1024; i++) {
        memset(&sml_info, 0, sizeof(sml_info));
        sprintf(sml_info.fname, "%d", i);
        sml_info.nlen = strlen(sml_info.fname);
        sprintf(sml_info.fcontent, "%d --> 1024", i);
        sml_info.clen = strlen(sml_info.fcontent);
        ret = write_seqfile(&seq_info, &sml_info);
        if (OPERATE_OK != ret) {
            return ret;
        }
    }

    srandom(time(NULL));
    for (i=0; i<10; i++) {
        memset(&sml_info, 0, sizeof(sml_info));
        sprintf(sml_info.fname, "%d", random()%1024);
        ret = read_seqfile_by_name(&seq_info, &sml_info);
        if (OPERATE_OK != ret) {
            return ret;
        }
        
        printf ("file name is [%s], content is [%s]\n", sml_info.fname, sml_info.fcontent);
    }
    
    ret = close_seqfile(&seq_info);
    if (OPERATE_OK != ret) {
        return ret;
    }
    
    return 0;
}
