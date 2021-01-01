#!/bin/bash

echo "// Files $1" >$2
echo "#include \"../main/html.h\"" >>$2
echo "#include <stddef.h>" >>$2
echo >>$2

for FILE in $1
do
    VAR=`echo "$FILE" | sed -e "s/[\.-]/_/g"`
    echo "extern uint8_t $VAR;" >>$2
    echo "extern uint16_t ${VAR}_length;" >>$2
done
echo >>$2

echo "struct HTML_FILE html_files[] = {" >>$2
for FILE in $1
do
    VAR=`echo "$FILE" | sed -e "s/[\.-]/_/g"`
    echo "  { .name = \"$FILE\", .size = &${VAR}_length, .data = &$VAR }, " >>$2
done
echo "  { NULL, 0, NULL }" >>$2
echo "};" >>$2
