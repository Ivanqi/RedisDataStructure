#ifndef LZF_2_H
#define LZF_2_H

#define LZF_VERSION 0x0105


/**
 * 压缩存储在内存块中从in_data开始的in_len字节，并将结果写入out_data，最大长度为out_len字
 * 
 * 如果输出缓冲区不够大或发生任何错误，则返回0，否则返回已使用的字节数
 * 这可能比in_len大得多（但小于原始大小的104%）
 * 因此始终使用out_len==in_len-1是有意义的
 * 以确保进行一些_压缩，并存储未压缩的数据
 * 
 * lzf_compress可能在不同的系统甚至不同的运行中使用不同的算法
 * 因此根据月球的相位或类似的因素，可能会产生不同的压缩字符串
 * 但是，所有这些字符串都是独立于体系结构的，并且在使用lzf_decompress解压缩时将生成原始数据。
 * 
 * 缓冲区不得重叠
 * 
 * 如果启用了选项LZF_STATE_ARG，则必须提供一个不会反映在该头文件中的额外参数
 * 参考lzfP.h和lzf_c.c。
 */
unsigned int lzf_compress(const void *const in_data, unsigned int in_len, void *out_data, unsigned int out_len);

/**
 * 解压使用某些版本的lzf_compress函数压缩的数据，并将其存储在“数据”位置，长度以“长度”为单位
 * 结果将存储在out_data中，最多存储out_len个字符
 * 
 * 如果输出缓冲区不足以容纳解压缩的数据，则返回0并将errno设置为E2BIG
 * 否则，将返回解压缩字节数（即数据的原始长度）
 * 
 * 如果在压缩数据中检测到错误，则返回零并将errno设置为EINVAL
 * 
 * 这个函数非常快，大约和复制循环一样快
 */
unsigned int lzf_decompress(const void *const in_data, unsigned int in_len, void *out_data, unsigned int out_len);
#endif