extern int s2636_x_offset;
extern int s2636_y_offset;

void s2636_w(unsigned char *workram,int offset,int data,unsigned char *dirty);
void Update_Bitmap(struct osd_bitmap *bitmap,unsigned char *workram,unsigned char *dirty,int Graphics_Bank,struct osd_bitmap *collision_bitmap);

