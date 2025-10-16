/* QLogic (R)NIC Driver/Library
 * Copyright (c) 2010-2017  Cavium, Inc.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HEX 16
#define WORD 8
#define H_WORD 4

enum FEATURE {
	IDLE_CHK = 1,
	GRC_DUMP = 2,
	MCP_TRACE = 3,
	REG_FIFO = 4,
	PROTECTION_OVERRIDE = 5,
	IGU_FIFO = 6,
	PHY = 7,
	FW_ASSERTS = 8,
	NVM_CFG1 = 9,
	DEFAULT_CFG = 10,
	NVM_META = 11,
	MDUMP = 12,
	ILT = 13,
	INTERNAL_TRACE = 14,
	LINKDUMP = 15,
} feature;

int engine;
int omit_engine;
int bin_dump;
int old_mode_vect_idx;
int idle_chk_files_num;
int grc_dump_files_num;
int mcp_trace_files_num;
int reg_fifo_files_num;
int igu_fifo_files_num;
int protection_override_files_num;
int fw_asserts_files_num;
int phy_files_num;
int nvm_cfg1_files_num;
int default_cfg_files_num;
int nvm_meta_files_num;
int mdump_files_num;
int linkdump_files_num;
int ilt_files_num;
int internal_trace_files_num;

FILE *idle_chk_files[20];
FILE *grc_dump_files[20];
FILE *mcp_trace_files[20];
FILE *reg_fifo_files[20];
FILE *igu_fifo_files[20];
FILE *protection_override_files[20];
FILE *fw_asserts_files[20];
FILE *phy_files[20];
FILE *nvm_cfg1_files[20];
FILE *default_cfg_files[20];
FILE *nvm_meta_files[20];
FILE *mdump_files[20];
FILE *linkdump_files[20];
FILE *ilt_files[20];
FILE *internal_trace_files[20];

void open_new_file(char *file_path);
void calculate_feature_and_engine(short input);

int main (int argc, char *argv[])
{
	int i;
	char *token;
	char buffer[4];
	size_t len = 0;
	FILE *source_file;
	char *line = NULL;
	long current_read_idx = 0;
	long next_item_start_idx = 0;

	/* open the source file for reading */
	source_file = fopen(argv[1],"r");
	if (source_file == NULL) {
		printf("ERROR OPENING FILE %s\n", argv[1]);
		return -1;
	}

	/* parse the file */
	while (getline(&line, &len, source_file) != -1) {
		token = strtok(line," \n");
		while (token != NULL) {

			/* get file size - adding H_WORD because of the 4 bytes that represent the header of the feature */
			if (current_read_idx < 3) {
				next_item_start_idx += strtol(token, NULL, HEX) << (WORD * current_read_idx);
				if (current_read_idx == 2)
					next_item_start_idx += H_WORD;
			} else if (current_read_idx == 3) {
				/* get the feature of the parsed part of the file and open file for writing */
				calculate_feature_and_engine(strtol(token, NULL, HEX));
				open_new_file(argv[2]);
			} else {
				switch (feature) {
				case IDLE_CHK:
					fputc(strtol(token, NULL, HEX), idle_chk_files[idle_chk_files_num]);
					break;
				case MCP_TRACE:
					fputc(strtol(token, NULL, HEX), mcp_trace_files[mcp_trace_files_num]);
					break;
				case REG_FIFO:
					fputc(strtol(token, NULL, HEX), reg_fifo_files[reg_fifo_files_num]);
					break;
				case PROTECTION_OVERRIDE:
					fputc(strtol(token, NULL, HEX), protection_override_files[protection_override_files_num]);
					break;
				case IGU_FIFO:
					fputc(strtol(token, NULL, HEX), igu_fifo_files[igu_fifo_files_num]);
					break;
				case FW_ASSERTS:
					fputc(strtol(token, NULL, HEX), fw_asserts_files[fw_asserts_files_num]);
					break;
				case PHY:
					fputc(strtol(token, NULL, HEX), phy_files[phy_files_num]);
					break;
				case GRC_DUMP:
					sprintf(buffer, "%c", strtol(token, NULL, HEX));
					fwrite(buffer, sizeof(char), 1, grc_dump_files[grc_dump_files_num]);
					break;
				case NVM_CFG1:
					sprintf(buffer, "%c", strtol(token, NULL, HEX));
					fwrite(buffer, sizeof(char), 1, nvm_cfg1_files[nvm_cfg1_files_num]);
					break;
				case DEFAULT_CFG:
					sprintf(buffer, "%c", strtol(token, NULL, HEX));
					fwrite(buffer, sizeof(char), 1, default_cfg_files[default_cfg_files_num]);
					break;
				case NVM_META:
					sprintf(buffer, "%c", strtol(token, NULL, HEX));
					fwrite(buffer, sizeof(char), 1, nvm_meta_files[nvm_meta_files_num]);
					break;
				case MDUMP:
					sprintf(buffer, "%c", strtol(token, NULL, HEX));
					fwrite(buffer, sizeof(char), 1, mdump_files[mdump_files_num]);
					break;
				case ILT:
					sprintf(buffer, "%c", strtol(token, NULL, HEX));
					fwrite(buffer, sizeof(char), 1, ilt_files[ilt_files_num]);
					break;
				case INTERNAL_TRACE:
					fputc(strtol(token, NULL, HEX), internal_trace_files[internal_trace_files_num]);
					break;
				case LINKDUMP:
					sprintf(buffer, "%c", strtol(token, NULL, HEX));
					fwrite(buffer, sizeof(char), 1, linkdump_files[linkdump_files_num]);
					break;
				}
			}

			current_read_idx++;

			/* get next token */
			token = strtok(NULL, " \n");

			/* Move to the next state when buffer is complete
			* Must make sure loop counter is after the part of buffer containing the size.
			*/
			if (current_read_idx == next_item_start_idx && current_read_idx > 3) {
				current_read_idx = 0;
				next_item_start_idx = 0;
				/* advance the counter of the freature that created */
				switch (feature) {
				case GRC_DUMP:
					grc_dump_files_num++;
					break;
				case IDLE_CHK:
					idle_chk_files_num++;
					break;
				case MCP_TRACE:
					mcp_trace_files_num++;
					break;
				case REG_FIFO:
					reg_fifo_files_num++;
					break;
				case PROTECTION_OVERRIDE:
					protection_override_files_num++;
					break;
				case IGU_FIFO:
					igu_fifo_files_num++;
					break;
				case FW_ASSERTS:
					fw_asserts_files_num++;
					break;
				case PHY:
					phy_files_num++;
					break;
				case NVM_CFG1:
					nvm_cfg1_files_num++;
					break;
				case DEFAULT_CFG:
					default_cfg_files_num++;
					break;
				case NVM_META:
					nvm_meta_files_num++;
					break;
				case MDUMP:
					mdump_files_num++;
					break;
				case ILT:
					ilt_files_num++;
					break;
				case INTERNAL_TRACE:
					internal_trace_files_num++;
					break;
				case LINKDUMP:
					linkdump_files_num++;
					break;
				}
			}
		}
	}

	printf("Slicing done. files created:\n%d grc_dump files\n%d idle_chk\n%d mcp_trace\n%d reg_fifo\n%d protection_override\n%d igu_fifo\n%d fw_asserts\n%d phy\n%d nvm_cfg1\n%d default_cfg\n%d nvm_meta\n%d mdump\n%d linkdump\n%d ilt\n%d internal_trace",
	       grc_dump_files_num, idle_chk_files_num, mcp_trace_files_num, reg_fifo_files_num, protection_override_files_num, igu_fifo_files_num, fw_asserts_files_num, phy_files_num, nvm_cfg1_files_num, default_cfg_files_num, nvm_meta_files_num, mdump_files_num, linkdump_files_num, ilt_files_num, internal_trace_files_num);

	/* close file handles */
	fclose(source_file);
	for (i = 0; i < idle_chk_files_num; i++)
		fclose(idle_chk_files[i]);
	for (i = 0; i < grc_dump_files_num; i++)
		fclose(grc_dump_files[i]);
	for (i = 0; i < mcp_trace_files_num; i++)
		fclose(mcp_trace_files[i]);
	for (i = 0; i < reg_fifo_files_num; i++)
		fclose(reg_fifo_files[i]);
	for (i = 0; i < protection_override_files_num; i++)
		fclose(protection_override_files[i]);
	for (i = 0; i < igu_fifo_files_num; i++)
		fclose(igu_fifo_files[i]);
	for (i = 0; i < fw_asserts_files_num; i++)
		fclose(fw_asserts_files[i]);
	for (i = 0; i < phy_files_num; i++)
		fclose(phy_files[i]);
	for (i = 0; i < nvm_cfg1_files_num; i++)
		fclose(nvm_cfg1_files[i]);
	for (i = 0; i < default_cfg_files_num; i++)
		fclose(default_cfg_files[i]);
	for (i = 0; i < nvm_meta_files_num; i++)
		fclose(nvm_meta_files[i]);
	for (i = 0; i < mdump_files_num; i++)
		fclose(mdump_files[i]);
	for (i = 0; i < linkdump_files_num; i++)
		fclose(linkdump_files[i]);
	for (i = 0; i < ilt_files_num; i++)
		fclose(ilt_files[i]);
	for (i = 0; i < internal_trace_files_num; i++)
		fclose(internal_trace_files[i]);

	return 0;
}

void open_new_file(char *file_path)
{
	char path[200], ext[8];

	if (bin_dump)
		strcpy(ext, "bin");
	else
		strcpy(ext, "txt");

	/* open files for writing according to the feature */
	switch (feature) {
	case IDLE_CHK:
		/* omit_engine = 1 means not in 100G mode, omit the engine number in the output files */
		if (omit_engine)
			sprintf(path, "%s/IdleChk%d.%s", file_path, idle_chk_files_num, ext);
		else
			sprintf(path, "%s/IdleChk%d-engine%d.%s", file_path, idle_chk_files_num, engine, ext);
		idle_chk_files[idle_chk_files_num] = fopen(path, "w");
		break;
	case REG_FIFO:
		/* omit_engine = 1 means not in 100G mode, omit the engine number in the output files */
		if (omit_engine)
			sprintf(path, "%s/RegFifo%d.%s", file_path, reg_fifo_files_num, ext);
		else
			sprintf(path, "%s/RegFifo%d-engine%d.%s", file_path, reg_fifo_files_num, engine, ext);
		reg_fifo_files[reg_fifo_files_num] = fopen(path, "w");
		break;
	case IGU_FIFO:
		/* omit_engine = 1 means not in 100G mode, omit the engine number in the output files */
		if (omit_engine)
			sprintf(path, "%s/IguFifo%d.%s", file_path, igu_fifo_files_num, ext);
		else
			sprintf(path, "%s/IguFifo%d-engine%d.%s", file_path, igu_fifo_files_num, engine, ext);
		igu_fifo_files[igu_fifo_files_num] = fopen(path, "w");
		break;
	case PROTECTION_OVERRIDE:
		/* omit_engine = 1 means not in 100G mode, omit the engine number in the output files */
		if (omit_engine)
			sprintf(path, "%s/ProtectionOverride%d.%s", file_path, protection_override_files_num, ext);
		else
			sprintf(path, "%s/ProtectionOverride%d-engine%d.%s", file_path, protection_override_files_num, engine, ext);
		protection_override_files[protection_override_files_num] = fopen(path, "w");
		break;
	case FW_ASSERTS:
		/* omit_engine = 1 means not in 100G mode, omit the engine number in the output files */
		if (omit_engine)
			sprintf(path, "%s/FwAsserts%d.%s", file_path, fw_asserts_files_num, ext);
		else
			sprintf(path, "%s/FwAsserts%d-engine%d.%s", file_path, fw_asserts_files_num, engine, ext);
		fw_asserts_files[fw_asserts_files_num] = fopen(path, "w");
		break;
	case GRC_DUMP:
		if (omit_engine)
			sprintf(path, "%s/GrcDump%d.bin", file_path, grc_dump_files_num);
		else
			sprintf(path, "%s/GrcDump%d-engine%d.bin", file_path, grc_dump_files_num, engine);
		grc_dump_files[grc_dump_files_num] = fopen(path, "wb");
		break;
	case PHY:
		sprintf(path, "%s/Phy%d.%s", file_path, phy_files_num, ext);
		phy_files[phy_files_num] = fopen(path, "w");
		break;
	case MCP_TRACE:
		sprintf(path, "%s/McpTrace%d.%s", file_path, mcp_trace_files_num, ext);
		mcp_trace_files[mcp_trace_files_num] = fopen(path, "w");
		break;
	case NVM_CFG1:
		sprintf(path, "%s/NvmCfg1%d.bin", file_path, nvm_cfg1_files_num);
		nvm_cfg1_files[nvm_cfg1_files_num] = fopen(path, "wb");
		break;
	case DEFAULT_CFG:
		sprintf(path, "%s/DefaultCfg%d.bin", file_path, default_cfg_files_num);
		default_cfg_files[default_cfg_files_num] = fopen(path, "wb");
		break;
	case NVM_META:
		sprintf(path, "%s/NvmMeta%d.bin", file_path, nvm_meta_files_num);
		nvm_meta_files[nvm_meta_files_num] = fopen(path, "wb");
		break;
	case MDUMP:
		sprintf(path, "%s/Mdump%d.bin", file_path, mdump_files_num);
		mdump_files[mdump_files_num] = fopen(path, "wb");
		break;
	case LINKDUMP:
		sprintf(path, "%s/LinkDump%d.%s", file_path, linkdump_files_num, ext);
		linkdump_files[linkdump_files_num] = fopen(path, "wb");
		break;
	case ILT:
		/* omit_engine = 1 means not in 100G mode, omit the engine number in the output files */
		if (omit_engine)
			sprintf(path, "%s/Ilt%d.bin", file_path, ilt_files_num);
		else
			sprintf(path, "%s/Ilt%d-engine%d.bin", file_path, ilt_files_num, engine);
		ilt_files[ilt_files_num] = fopen(path, "wb");
		break;
	case INTERNAL_TRACE:
		sprintf(path, "%s/internalTrace%d.txt", file_path, internal_trace_files_num);
		internal_trace_files[internal_trace_files_num] = fopen(path, "wb");
		break;
	default:
		if (feature != 0)
			printf("%d feature doesn't exist!\n",feature);
	}
}

void calculate_feature_and_engine(short input)
{
	/* from received feature learn which engine is it and remove it from the feature */
	engine = input >> 7;
	feature = input & 0x1f;
	omit_engine = (input >> 6) & 0x01;
	bin_dump = (input >> 5) & 0x01;
}
