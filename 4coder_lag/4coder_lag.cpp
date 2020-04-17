
void dumbPrintNum(Application_Links* app, i64 num)
{
#define dumbSw(raw) case raw: log_string(app, string_u8_litexpr(#raw)); break
    switch (num)
    {
        dumbSw(-1);
        dumbSw(0);
        dumbSw(1);
        dumbSw(2);
        dumbSw(3);
        dumbSw(4);
        dumbSw(5);
        dumbSw(6);
        dumbSw(7);
        dumbSw(8);
        dumbSw(9);
        dumbSw(10);
        dumbSw(11);
        default: break;
    }
}

bool readChar(Application_Links* app, Buffer_ID buffer, i64 ind, u8& val)
{
    u8 tBuffer[8];
    
    Range_i64 r;
    r.start = ind;
    r.end = ind + 1;
    
    bool ret = buffer_read_range(app, buffer, r, tBuffer);
    if (ret)
    {
        val = tBuffer[0];
    }
    return ret;
}

i64 bufferReadNumEndColon(Application_Links* app, Buffer_ID buffer, i64 begin, i64& endPos)
{
    i64 i = 0;
    u8 vals[256];
    while (readChar(app, buffer, begin + i, vals[i]))
    {
        if (vals[i] == ':')
        {
            endPos = begin + i;
            String_Const_u8 str;
            str.str = vals;
            str.size = i;
            u64 ret = string_to_integer(str, 10);
            return ret;
        }
        ++i;
        if (i >= 256)
            return 0;
    }
    return 0;
}

bool bufferMatchString(Application_Links* app, Buffer_ID buffer, i64 begin, String_Const_u8 str)
{
    if (str.size >= 256)
        return false;
    
    u8 tBuffer[256];
    Range_i64 r;
    r.start = begin;
    r.end = begin + str.size;
    if (buffer_read_range(app, buffer, r, tBuffer))
    {
        String_Const_u8 oStr;
        oStr.str = tBuffer;
        oStr.size = str.size;
        return string_compare(oStr, str) == 0;
    }
    return false;
}

enum ErrorType
{
    ERRORTYPE_ERROR,
    ERRORTYPE_FATAL_ERROR,
    ERRORTYPE_WARNING
};

using ErrFunc = void(*)(Application_Links*, i64 lineNum, String_Const_u8 file);

void findError(Application_Links* app, i64 number, ErrFunc callback, bool runAll = false)
{
    Buffer_ID buffer = get_buffer_by_name(app, string_u8_litexpr("*compilation*"), 0);
    
    log_string(app, string_u8_litexpr("Try to find compilation thing"));
    
    i64 errorInd = 0;
    
    u8 val;
    i64 i = 0;
    i64 lineStart = 0;
    i64 lastPathStart = 0;
    i64 lastPathEnd = 0;
    i64 lineNum = 0;
    bool seekEndline = false;
    bool isMessage = false;
    while (readChar(app, buffer, i, val))
    {
        if (val == '\n')
        {
            isMessage = false;
            seekEndline = false;
            lineStart = i + 1;
            lastPathStart = i + 1;
            lastPathEnd = i + 1;
            ++lineNum;
            ++i;
            continue;
        }
        
        if (seekEndline)
        {
            ++i;
            continue;
        }
        
        if (isMessage)
        {
            u8 tBuffer[256];
            Range_i64 r;
            r.start = lastPathStart;
            r.end = lastPathEnd;
            buffer_read_range(app, buffer, r, tBuffer);
            String_Const_u8 str;
            str.str = tBuffer;
            str.size = r.end - r.start;
            
            if (str.size > 1)
            {
                log_string(app, string_u8_litexpr("\n"));
                log_string(app, str);
                log_string(app, string_u8_litexpr("\n"));
                i64 errLineNumEnd;
                i64 errLineNum = bufferReadNumEndColon(app, buffer, r.end + 1, errLineNumEnd);
                
                i64 errColonNumEnd;
                /*i64 errColonNum =*/ bufferReadNumEndColon(app, buffer, errLineNumEnd + 1, errColonNumEnd);
                
                // Check if warning
                if (!bufferMatchString(app, buffer, errColonNumEnd + 1, string_u8_litexpr(" warning")))
                {
                    if (errorInd == number || runAll)
                    {
                        callback(app, errLineNum, str);
                        if (!runAll)
                            return;
                    }
                    else
                    {
                        ++errorInd;
                    }
                }
                else
                {
                }
            }
            
            seekEndline = true;
        }
        else
        {
            if (val == '>')
            {
                seekEndline = true;
            }
            else if (val == ':')
            {
                // Start of message
                isMessage = true;
                lastPathEnd = i;
            }
            else if (val == '\\' || val == '/')
            {
                lastPathStart = i + 1;
            }
        }
        
        ++i;
    }
}

struct ErrorMessage {
    i64 lineNum;
    Buffer_ID buffer;
};

ErrorMessage errors[256];
i64 errorsCount = 0;

i64 lagErrorInd = 0;
i64 lagBuildNum = 0;

void pushError(Application_Links* app, i64 lineNum, String_Const_u8 file)
{
    if (errorsCount < 256)
    {
        Buffer_ID buffer = get_buffer_by_name(app, file, 0);
        errors[errorsCount++] = { lineNum, buffer };
    }
}

void jumpNextError(Application_Links* app, i64 lineNum, String_Const_u8 file)
{
    View_ID view = get_active_view(app, 0);
    
    if (view_set_buffer(app, view, get_buffer_by_name(app, file, 0), SetBuffer_KeepOriginalGUI))
    {
        Buffer_Seek s;
        s.type = buffer_seek_line_col;
        s.line = lineNum;
        s.col = 0;
        view_set_cursor(app, view, s);
    }
}

CUSTOM_COMMAND_SIG(lag_goto_next_error)
CUSTOM_DOC("Go to the next comilation error.")
{
    ++lagErrorInd;
    dumbPrintNum(app, lagErrorInd);
    findError(app, lagErrorInd, &jumpNextError);
}

CUSTOM_COMMAND_SIG(lag_goto_previous_error)
CUSTOM_DOC("Go to the previous comilation error.")
{
    if (lagErrorInd > 0)
        --lagErrorInd;
    dumbPrintNum(app, lagErrorInd);
    findError(app, lagErrorInd, &jumpNextError);
}

CUSTOM_COMMAND_SIG(lag_build_search)
CUSTOM_DOC("Search for build file and run it. Also resets error index.")
{
    lagErrorInd = -1;
    ++lagBuildNum;
    errorsCount = 0;
    build_search(app);
}

CUSTOM_COMMAND_SIG(lag_load_error_display)
CUSTOM_DOC("Load the error display data.")
{
    findError(app, 256, &pushError, true);
}

bool isType(String_Const_u8 str);