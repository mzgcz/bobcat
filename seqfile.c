#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/types.h>

#include "seqfile.h"

#define SEQ_VERSION 1
#define SEQ_SYNCTAG 255
#define SEQ_END_NO 0
#define SEQ_END_YES 1

void encrypt_str(char *str, uint8_t len)
{
    uint8_t i;
    
    for (i=0; i<len; i++) {
        str[i] ^= (len-i);
    }
}

void write_seq_head(FILE *seq_fd)
{
    time_t lt;
    char tag[4];
    uint8_t version;
    uint8_t sync_tag;
    uint8_t sync_len;
    char sync_str[64];
    
    memset(tag, 0, sizeof(tag));
    strcpy(tag, "SEQ");
    fwrite(tag, strlen(tag), 1, seq_fd);
    
    version = SEQ_VERSION;
    fwrite(&version, sizeof(version), 1, seq_fd);
    
    sync_tag = SEQ_SYNCTAG;
    fwrite(&sync_tag, sizeof(sync_tag), 1, seq_fd);
    
    lt = time(NULL);
    memset(sync_str, 0, sizeof(sync_str));
    sprintf(sync_str, "%ld@%ld", lt, lt);

    sync_len = strlen(sync_str);
    fwrite(&sync_len, sizeof(sync_len), 1, seq_fd);
    
    encrypt_str(sync_str, sync_len);
    fwrite(sync_str, sync_len, 1, seq_fd);
}

int8_t check_seq_info(struct seqfile_info *seq_info)
{
    uint8_t len;
    FILE *seq_fd;
    struct stat64 buff;
    char seq_name[512];
    
    len = strlen(seq_info->pdir);
    if (0==len || len>=128) {
        return SEQFILE_DIRERR;
    }
    memset(&buff, 0, sizeof(buff));
    if (stat64(seq_info->pdir,&buff)<0 || !S_ISDIR(buff.st_mode)) {
        return SEQFILE_DIRERR;
    }

    len = strlen(seq_info->index_dir);
    if (0 == len) {
        strcpy(seq_info->index_dir, seq_info->pdir);
    } else if (len >= 128) {
        return INDEXFILE_DIRERR;
    } else {
        memset(&buff, 0, sizeof(buff));
        if (stat64(seq_info->index_dir,&buff)<0 || !S_ISDIR(buff.st_mode)) {
            return INDEXFILE_DIRERR;
        }
    }
    
    len = strlen(seq_info->fname);
    if (0==len || len>=128) {
        return SEQFILE_NAMERR;
    }
    memset(seq_name, 0, sizeof(seq_name));
    sprintf(seq_name, "%s/%s", seq_info->pdir, seq_info->fname);
    memset(&buff, 0, sizeof(buff));
    if (ERROR_WHEN_NONENTITY == seq_info->crt_tag) {
        if (stat64(seq_name,&buff) < 0) {
            return SEQFILE_NONENTITYERR;
        }
    } else if (CREATE_WHEN_NONENTITY == seq_info->crt_tag) {
        if (stat64(seq_name,&buff) < 0) {
            seq_fd = fopen64(seq_name, "wb");
            if (!seq_fd) {
                return SEQFILE_CREATERR;
            }
            write_seq_head(seq_fd);
            if (fclose(seq_fd)) {
                return SEQFILE_CLOSERR;
            }
        }
    } else {
        return SEQFILE_CRTAGERR;
    }
    
    if (stat64(seq_name,&buff)<0 || !S_ISREG(buff.st_mode)) {
        return SEQFILE_NAMERR;
    }
    
    return OPERATE_OK;
}

int8_t aligning_seqfile(struct seqfile_info *seq_info, char *seq_name)
{
    int16_t i;
    char tag[4];
    FILE *seq_fd;
    uint64_t len;
    uint8_t version;
    uint8_t sync_tag;
    uint8_t sync_len;
    time_t lt_1, lt_2;
    char sync_str[64];
    int64_t pos, llen;
    char content[1024];
    uint8_t ret = OPERATE_OK;
    
    seq_fd = fopen64(seq_name, "rb+");
    if (!seq_fd) {
        return SEQFILE_OPENERR;
    }
    
    memset(tag, 0, sizeof(tag));
    fread(tag, 3, 1, seq_fd);
    if (0 != strcmp(tag,"SEQ")) {
        ret = SEQFILE_TAGERR;
        goto ALIGNING_ERR;
    }
    
    fread(&version, sizeof(version), 1, seq_fd);
    if (SEQ_VERSION != version) {
        ret = SEQFILE_VERSIONERR;
        goto ALIGNING_ERR;
    }
    
    fread(&sync_tag, sizeof(sync_tag), 1, seq_fd);
    if (SEQ_SYNCTAG != sync_tag) {
        ret = SEQFILE_SYNCTAGERR;
        goto ALIGNING_ERR;
    }
    
    fread(&sync_len, sizeof(sync_len), 1, seq_fd);
    if (sync_len >= 64) {
        ret = SEQFILE_SYNCLENERR;
        goto ALIGNING_ERR;
    }
    
    memset(sync_str, 0, sizeof(sync_str));
    fread(sync_str, sync_len, 1, seq_fd);
    encrypt_str(sync_str, sync_len);
    if (strlen(sync_str) != sync_len) {
        ret = SEQFILE_SYNCSTRERR;
        goto ALIGNING_ERR;
    }
    sscanf(sync_str, "%ld@%ld", &lt_1, &lt_2);
    if (lt_1 != lt_2) {
        ret = SEQFILE_SYNCSTRERR;
        goto ALIGNING_ERR;
    }
    
    seq_info->sync_len = sync_len;
    encrypt_str(sync_str, sync_len);
    memcpy(seq_info->sync_str, sync_str, sync_len);
    
    pos = ftello64(seq_fd);
    if (pos < 0) {
        ret = SEQFILE_GETPOSERR;
        goto ALIGNING_ERR;
    }
    seq_info->head_len = pos;
    
    if (fseeko64(seq_fd,0,SEEK_END) < 0) {
        ret = SEQFILE_SEEKERR;
        goto ALIGNING_ERR;
    }
    llen = ftello64(seq_fd);
    if (llen < 0) {
        ret = SEQFILE_GETPOSERR;
        goto ALIGNING_ERR;
    }
    while (1) {
        llen -= 1024;
        len = (llen>0)?llen:0;
        if (fseeko64(seq_fd,len,SEEK_SET) < 0) {
            ret = SEQFILE_SEEKERR;
            goto ALIGNING_ERR;
        }
        
        memset(content, 0, sizeof(content));
        fread(content, 1024, 1, seq_fd);
        for (i=1023; i>=0; i--) {
            if (SEQ_SYNCTAG == (uint8_t)(content[i])) {
                if (i+1>1023 || seq_info->sync_len!=content[i+1]) {
                    continue;
                } else if (i+1+content[i+1] > 1023) {
                    continue;
                } else if (0 != memcmp(content+i+2,seq_info->sync_str,seq_info->sync_len)) {
                    continue;
                } else {
                    pos = len+i+2+content[i+1];
                    goto ALIGNING_OK;
                }
            }
        }
        
        if (llen <= 0) {
            ret = SEQFILE_FORMERR;
            goto ALIGNING_ERR;
        } else {
            llen += 64;
        }
    }

ALIGNING_OK:
    if (ftruncate64(fileno(seq_fd),pos) < 0) {
        if (fclose(seq_fd)) {
            return SEQFILE_CLOSERR;
        }
        return SEQFILE_TRUNCATERR;
    }
    
    if (fclose(seq_fd)) {
        return SEQFILE_CLOSERR;
    }
    
    seq_info->end_pos = pos;
    
    return OPERATE_OK;
    
ALIGNING_ERR:
    if (fclose(seq_fd)) {
        return SEQFILE_CLOSERR;
    }
    
    return ret;
}

int get_indexinfo(void *info, int count, char **value, char **name)
{
    int i;
    struct indexfile_info *index_info;
    
    index_info = (struct indexfile_info *)info;
    index_info->num++;
    for (i=0; i<count; i++) {
        if (0 == strcmp(name[i],"pos")) {
            sscanf(value[i], "%llu", &(index_info->pos));
        } else if (0 == strcmp(name[i],"len")) {
            sscanf(value[i], "%llu", &(index_info->len));
        }
    }
    
    return 0;
}

int8_t fill_indexfile(struct seqfile_info *seq_info, uint64_t index_end_pos)
{
    int8_t ret;
    uint64_t last_pos;
    uint32_t last_len;
    char sql_str[256];
    char index_name[512];
    int64_t beg_pos, end_pos;
    struct smlfile_info sml_info;
    
    if (fseeko64(seq_info->fd,index_end_pos,SEEK_SET) < 0) {
        if (fclose(seq_info->fd)) {
            return SEQFILE_CLOSERR;
        }
        return SEQFILE_SEEKERR;
    }
    seq_info->end_tag = SEQ_END_NO;
    
    memset(sql_str, 0, sizeof(sql_str));
    strcpy(sql_str, "begin transaction;");
    if (SQLITE_OK != sqlite3_exec(seq_info->index_db,sql_str,NULL,NULL,&(seq_info->err_msg))) {
        return INDEXFILE_EXECERR;
    }
    beg_pos = 0;
    end_pos = 0;
    while (1) {
        if (0==beg_pos && 0==end_pos) {
            beg_pos = ftello64(seq_info->fd);
            if (beg_pos < 0) {
                if (fclose(seq_info->fd)) {
                    return SEQFILE_CLOSERR;
                }
                return SEQFILE_GETPOSERR;
            }
        } else {
            beg_pos = end_pos;
        }
        memset(&sml_info, 0, sizeof(sml_info));
        ret = read_seqfile_order(seq_info, &sml_info);
        if (SEQFILE_ENDOFILERR == ret) {
            seq_info->end_tag = SEQ_END_YES;
            break;
        } else if (OPERATE_OK != ret) {
            return ret;
        }
        end_pos = ftello64(seq_info->fd);
        if (end_pos < 0) {
            if (fclose(seq_info->fd)) {
                return SEQFILE_CLOSERR;
            }
            return SEQFILE_GETPOSERR;
        }
        
        memset(sql_str, 0, sizeof(sql_str));
        sprintf(sql_str, "replace into seq_index (name, pos, len) values ('%s', %llu, %llu);", sml_info.fname, beg_pos, end_pos-beg_pos);
        if (SQLITE_OK != sqlite3_exec(seq_info->index_db,sql_str,NULL,NULL,&(seq_info->err_msg))) {
            return INDEXFILE_EXECERR;
        }
        
        last_pos = beg_pos;
        last_len = end_pos-beg_pos;
    }
    memset(index_name, 0, sizeof(index_name));
    sprintf(index_name, "%s/.index_%s", seq_info->index_dir, seq_info->fname);
    memset(sql_str, 0, sizeof(sql_str));
    sprintf(sql_str, "replace into seq_index (name, pos, len) values ('%s', %llu, %llu);", index_name, last_pos, last_len);
    if (SQLITE_OK != sqlite3_exec(seq_info->index_db,sql_str,NULL,NULL,&(seq_info->err_msg))) {
        return INDEXFILE_EXECERR;
    }
    memset(sql_str, 0, sizeof(sql_str));
    strcpy(sql_str, "commit transaction;");
    if (SQLITE_OK != sqlite3_exec(seq_info->index_db,sql_str,NULL,NULL,&(seq_info->err_msg))) {
        return INDEXFILE_EXECERR;
    }
    
    return OPERATE_OK;
}

int8_t sync_indexfile(struct seqfile_info *seq_info)
{
    char sql_str[1024];
    char index_name[512];
    uint64_t index_end_pos;
    struct indexfile_info index_info;
    
    memset(index_name, 0, sizeof(index_name));
    sprintf(index_name, "%s/.index_%s", seq_info->index_dir, seq_info->fname);
    
    if (SQLITE_OK != sqlite3_open(index_name, &(seq_info->index_db))) {
        return INDEXFILE_OPENERR;
    }
    
    memset(sql_str, 0, sizeof(sql_str));
    strcpy(sql_str, "create table if not exists seq_index (name text primary key, pos integer, len integer)");
    if (SQLITE_OK != sqlite3_exec(seq_info->index_db,sql_str,NULL,NULL,&(seq_info->err_msg))) {
        return INDEXFILE_EXECERR;
    }
    
    memset(sql_str, 0, sizeof(sql_str));
    sprintf(sql_str, "select pos, len from seq_index where name='%s'", index_name);
    memset(&index_info, 0, sizeof(index_info));
    if (SQLITE_OK != sqlite3_exec(seq_info->index_db,sql_str,get_indexinfo,&index_info,&(seq_info->err_msg))) {
        return INDEXFILE_EXECERR;
    }
    
    if (0 == index_info.num) {
        memset(sql_str, 0, sizeof(sql_str));
        sprintf(sql_str, "insert into seq_index values('%s', %llu, 0)", index_name, seq_info->head_len);
        if (SQLITE_OK != sqlite3_exec(seq_info->index_db,sql_str,NULL,NULL,&(seq_info->err_msg))) {
            return INDEXFILE_EXECERR;
        }
        index_info.pos = seq_info->head_len;
        index_info.len = 0;
    } else if (1 == index_info.num) {
        index_end_pos = index_info.pos+index_info.len;
        if (seq_info->end_pos > index_end_pos) {
            fill_indexfile(seq_info, index_end_pos);
        } else if (seq_info->end_pos < index_end_pos) {
            return INDEXFILE_OVERTOPERR;
        }
    } else {
        return INDEXFILE_INFOERR;
    }
    
    return OPERATE_OK;
}

int8_t open_seqfile(struct seqfile_info *seq_info)
{
    int8_t ret;
    FILE *seq_fd;
    char seq_name[512];

    if (!seq_info) {
        return SEQFILE_INFOERR;
    }
    
    ret = check_seq_info(seq_info);
    if (OPERATE_OK != ret) {
        return ret;
    }
    
    memset(seq_name, 0, sizeof(seq_name));
    sprintf(seq_name, "%s/%s", seq_info->pdir, seq_info->fname);
    ret = aligning_seqfile(seq_info, seq_name);
    if (OPERATE_OK != ret) {
        return ret;
    }
    
    seq_fd = fopen64(seq_name, "rb+");
    if (!seq_fd) {
        return SEQFILE_OPENERR;
    }
    if (fseeko64(seq_fd,0,SEEK_END) < 0) {
        if (fclose(seq_fd)) {
            return SEQFILE_CLOSERR;
        }
        return SEQFILE_SEEKERR;
    }
    
    seq_info->end_tag = SEQ_END_YES;
    seq_info->fd = seq_fd;
    
    ret = sync_indexfile(seq_info);
    if (OPERATE_OK != ret) {
        return ret;
    }
    
    return OPERATE_OK;
}

int8_t write_seqfile(struct seqfile_info *seq_info, struct smlfile_info *sml_info)
{
    int8_t ret;
    uint32_t net_len;
    uint8_t sync_tag;
    uint64_t beg_pos;
    
    if (!seq_info || !(seq_info->fd)) {
        return SEQFILE_INFOERR;
    }
    
    if (!sml_info) {
        return SMLFILE_INFOERR;
    }

    if (sml_info->nlen >= 256) {
        return SMLFILE_NAMERR;
    }

    if (sml_info->clen >= 0x100000L) {
        return SMLFILE_CONTENTERR;
    }
    
    if (SEQ_END_YES != seq_info->end_tag) {
        if (fseeko64(seq_info->fd,0,SEEK_END) < 0) {
            if (fclose(seq_info->fd)) {
                return SEQFILE_CLOSERR;
            }
            return SEQFILE_SEEKERR;
        }
        
        seq_info->end_tag = SEQ_END_YES;
    }
    
    beg_pos = seq_info->end_pos;
    
    net_len = htonl(sml_info->nlen);
    fwrite(&net_len, sizeof(net_len), 1, seq_info->fd);
    seq_info->end_pos += sizeof(net_len);
    
    net_len = htonl(sml_info->clen);
    fwrite(&net_len, sizeof(net_len), 1, seq_info->fd);
    seq_info->end_pos += sizeof(net_len);
    
    fwrite(sml_info->fname, sml_info->nlen, 1, seq_info->fd);
    seq_info->end_pos += sml_info->nlen;
    
    fwrite(sml_info->fcontent, sml_info->clen, 1, seq_info->fd);
    seq_info->end_pos += sml_info->clen;
    
    sync_tag = SEQ_SYNCTAG;
    fwrite(&sync_tag, sizeof(sync_tag), 1, seq_info->fd);
    seq_info->end_pos += sizeof(sync_tag);
    
    fwrite(&(seq_info->sync_len), sizeof(seq_info->sync_len), 1, seq_info->fd);
    seq_info->end_pos += sizeof(seq_info->sync_len);
    
    fwrite(seq_info->sync_str, seq_info->sync_len, 1, seq_info->fd);
    seq_info->end_pos += seq_info->sync_len;

    strcpy(seq_info->info[seq_info->cache_num].fname, sml_info->fname);
    seq_info->info[seq_info->cache_num].pos = beg_pos;
    seq_info->info[seq_info->cache_num].len = seq_info->end_pos-beg_pos;

    seq_info->cache_num++;
    if (seq_info->cache_num >= INDEX_CACHE_NUM) {
        ret = flush_seqfile(seq_info);
        if (OPERATE_OK != ret) {
            return ret;
        }
    }
    
    return OPERATE_OK;
}

int8_t flush_indexfile(struct seqfile_info *seq_info)
{
    uint16_t i;
    uint64_t last_pos;
    uint32_t last_len;
    char sql_str[256];
    char index_name[512];
    
    memset(sql_str, 0, sizeof(sql_str));
    strcpy(sql_str, "begin transaction;");
    if (SQLITE_OK != sqlite3_exec(seq_info->index_db,sql_str,NULL,NULL,&(seq_info->err_msg))) {
        return INDEXFILE_EXECERR;
    }
    for (i=0; i<seq_info->cache_num; i++) {
        memset(sql_str, 0, sizeof(sql_str));
        sprintf(sql_str, "replace into seq_index (name, pos, len) values ('%s', %llu, %llu);",
                seq_info->info[i].fname, seq_info->info[i].pos, seq_info->info[i].len);
        if (SQLITE_OK != sqlite3_exec(seq_info->index_db,sql_str,NULL,NULL,&(seq_info->err_msg))) {
            return INDEXFILE_EXECERR;
        }
        last_pos = seq_info->info[i].pos;
        last_len = seq_info->info[i].len;
    }
    memset(index_name, 0, sizeof(index_name));
    sprintf(index_name, "%s/.index_%s", seq_info->index_dir, seq_info->fname);
    memset(sql_str, 0, sizeof(sql_str));
    sprintf(sql_str, "replace into seq_index (name, pos, len) values ('%s', %llu, %llu);", index_name, last_pos, last_len);
    if (SQLITE_OK != sqlite3_exec(seq_info->index_db,sql_str,NULL,NULL,&(seq_info->err_msg))) {
        return INDEXFILE_EXECERR;
    }
    memset(sql_str, 0, sizeof(sql_str));
    strcpy(sql_str, "commit transaction;");
    if (SQLITE_OK != sqlite3_exec(seq_info->index_db,sql_str,NULL,NULL,&(seq_info->err_msg))) {
        return INDEXFILE_EXECERR;
    }
    
    seq_info->cache_num = 0;
    memset(seq_info->info, 0, INDEX_CACHE_NUM*sizeof(struct indexfile_info));
    
    return OPERATE_OK;
}

int8_t flush_seqfile(struct seqfile_info *seq_info)
{
    int8_t ret;
    
    if (!seq_info || !(seq_info->fd)) {
        return SEQFILE_INFOERR;
    }

    if (0 == seq_info->cache_num) {
        return OPERATE_OK;
    }
    
    if (fflush(seq_info->fd)) {
        return SEQFILE_FLUSHERR;
    }
    
    ret = flush_indexfile(seq_info);
    if (OPERATE_OK != ret) {
        return ret;
    }
    
    return OPERATE_OK;
}

int8_t goto_1st_smlfile(struct seqfile_info *seq_info)
{
    if (!seq_info || !(seq_info->fd)) {
        return SEQFILE_INFOERR;
    }
    
    if (fseeko64(seq_info->fd,seq_info->head_len,SEEK_SET) < 0) {
        if (fclose(seq_info->fd)) {
            return SEQFILE_CLOSERR;
        }
        return SEQFILE_SEEKERR;
    }
    
    seq_info->end_tag = SEQ_END_NO;
    
    return OPERATE_OK;
}

int8_t read_seqfile_order(struct seqfile_info *seq_info, struct smlfile_info *sml_info)
{
    int64_t now_pos;
    uint8_t sync_len;
    uint8_t sync_tag;
    char sync_str[64];
    
    if (!seq_info || !(seq_info->fd)) {
        return SEQFILE_INFOERR;
    }
    
    if (!sml_info) {
        return SMLFILE_INFOERR;
    }
    
    now_pos = ftello64(seq_info->fd);
    if (now_pos < 0) {
        if (fclose(seq_info->fd)) {
            return SEQFILE_CLOSERR;
        }
        return SEQFILE_GETPOSERR;
    }
    if (now_pos >= seq_info->end_pos) {
        return SEQFILE_ENDOFILERR;
    }

    fread(&(sml_info->nlen), sizeof(sml_info->nlen), 1, seq_info->fd);
    sml_info->nlen = ntohl(sml_info->nlen);
    
    fread(&(sml_info->clen), sizeof(sml_info->clen), 1, seq_info->fd);
    sml_info->clen = ntohl(sml_info->clen);
    
    fread(sml_info->fname, sml_info->nlen, 1, seq_info->fd);

    fread(sml_info->fcontent, sml_info->clen, 1, seq_info->fd);

    fread(&sync_tag, sizeof(sync_tag), 1, seq_info->fd);

    fread(&sync_len, sizeof(sync_len), 1, seq_info->fd);

    memset(sync_str, 0, sizeof(sync_str));
    fread(sync_str, sync_len, 1, seq_info->fd);
    
    return OPERATE_OK;
}

int8_t read_seqfile_by_name(struct seqfile_info *seq_info, struct smlfile_info *sml_info)
{
    int8_t ret;
    char sql_str[512];
    struct indexfile_info index_info;
    
    if (!seq_info || !(seq_info->fd)) {
        return SEQFILE_INFOERR;
    }
    
    if (!sml_info) {
        return SMLFILE_INFOERR;
    }

    ret = flush_seqfile(seq_info);
    if (OPERATE_OK != ret) {
        return ret;
    }
    
    memset(sql_str, 0, sizeof(sql_str));
    sprintf(sql_str, "select pos, len from seq_index where name='%s'", sml_info->fname);
    memset(&index_info, 0, sizeof(index_info));
    if (SQLITE_OK != sqlite3_exec(seq_info->index_db,sql_str,get_indexinfo,&index_info,&(seq_info->err_msg))) {
        return INDEXFILE_EXECERR;
    }
    
    if (1 == index_info.num) {
        if (fseeko64(seq_info->fd,index_info.pos,SEEK_SET) < 0) {
            if (fclose(seq_info->fd)) {
                return SEQFILE_CLOSERR;
            }
            return SEQFILE_SEEKERR;
        }
        seq_info->end_tag = SEQ_END_NO;
        ret = read_seqfile_order(seq_info, sml_info);
        if (OPERATE_OK != ret) {
            return ret;
        }
    } else if (0 == index_info.num) {
        return SMLFILE_NONENTITY;
    }else {
        return INDEXFILE_INFOERR;
    }
    
    return OPERATE_OK;
}

int8_t close_seqfile(struct seqfile_info *seq_info)
{
    int8_t ret;
    
    if (!seq_info || !(seq_info->fd)
        || !(seq_info->index_db)) {
        return SEQFILE_INFOERR;
    }

    ret = flush_seqfile(seq_info);
    if (OPERATE_OK != ret) {
        return ret;
    }
    
    if (fclose(seq_info->fd)) {
        return SEQFILE_CLOSERR;
    }
    
    if (SQLITE_OK != sqlite3_close(seq_info->index_db)) {
        return INDEXFILE_CLOSERR;
    }
    
    return OPERATE_OK;
}
