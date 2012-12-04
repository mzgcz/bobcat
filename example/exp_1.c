#include <string.h>
#include <seqfile.h>

int main(int argc, char *argv[])
{
    int8_t ret;
    struct seqfile_info seq_info;
    struct smlfile_info sml_info;
    
    memset(&seq_info, 0, sizeof(seq_info));
    strcpy(seq_info.pdir, "/tmp");
    strcpy(seq_info.fname, "exp_seq_1");
    seq_info.crt_tag = CREATE_WHEN_NONENTITY;
    ret = open_seqfile(&seq_info);
    if (OPERATE_OK != ret) {
        return ret;
    }
    
    memset(&sml_info, 0, sizeof(sml_info));
    strcpy(sml_info.fname, "test_file");
    sml_info.nlen = strlen(sml_info.fname);
    strcpy(sml_info.fcontent, "This is my test file!");
    sml_info.clen = strlen(sml_info.fcontent);
    ret = write_seqfile(&seq_info, &sml_info);
    if (OPERATE_OK != ret) {
        return ret;
    }
    
    ret = flush_seqfile(&seq_info);
    if (OPERATE_OK != ret) {
        return ret;
    }
    
    ret = goto_1st_smlfile(&seq_info);
    if (OPERATE_OK != ret) {
        return ret;
    }
    
    memset(&sml_info, 0, sizeof(sml_info));
    ret = read_seqfile_order(&seq_info, &sml_info);
    if (OPERATE_OK != ret) {
        return ret;
    }
    
    ret = close_seqfile(&seq_info);
    if (OPERATE_OK != ret) {
        return ret;
    }
    
    printf ("my test file name is [%s], content is [%s]\n", sml_info.fname, sml_info.fcontent);
    
    return 0;
}
