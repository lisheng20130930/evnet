#ifndef UPLOAD_H
#define UPLOAD_H


bool upload_checkURL(struct node_s *node);
bool upload_handleHeader(struct node_s *node);
int  upload_bodyContinue(struct node_s *node, char *data, int len);
void upload_clear(node_t *node);

char* upload_pathName(char *name);

#endif