#include <stdio.h>
#include <stdlib.h>

#include "sequence.h"
#include "command.h"
#include "background.h"
#include "lexer.h"

int sequence_command(struct token *current)
{
    while (current != NULL)
    {
        struct token *start = current;
        while (current != NULL &&
               current->type != OP_SEMI &&
               current->type != OP_AMP)
        {
            current = current->next;
        }
        if (current != NULL && current->type == OP_AMP)
        {
            if (has_pipeline(start))
            {
                struct pipeline_stage *stages;
                int stage_count;

                if (!parse_pipeline(start, &stages, &stage_count))
                {
                    printf("cshell: invalid syntax\n");
                }
                else
                {
                    run_pipeline_background(stages, stage_count);
                    free(stages);
                }
            }
            else
            {
                exec_background_command(start);
            }
        }
        else
        {
            int res = exec_command(start);
            if (res != 0)
                return res;
        }

        if (current == NULL)
            break;

        current = current->next;
    }

    return 0;
}
