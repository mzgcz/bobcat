# bobcat #
> *Four Steps To Access Small Files In High Speed*

----------

- **open file**

> `int8_t open_seqfile(struct seqfile_info *seq_info)`

- **write file**

> `int8_t write_seqfile(struct seqfile_info *seq_info, struct smlfile_info *sml_info)`

> `int8_t flush_seqfile(struct seqfile_info *seq_info)`

- **read file**

> `int8_t goto_1st_smlfile(struct seqfile_info *seq_info)`

> `int8_t read_seqfile_order(struct seqfile_info *seq_info, struct smlfile_info *sml_info)`

> `int8_t read_seqfile_by_name(struct seqfile_info *seq_info, struct smlfile_info *sml_info)`

- **close file**

> `int8_t close_seqfile(struct seqfile_info *seq_info)`
