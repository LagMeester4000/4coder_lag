
struct StructDef;

struct TypeMapString {
    u8 str[256];
    u64 size = 0;
};

enum StructMemberType {
    STRUCTMEMBER_VARIABLE,
    STRUCTMEMBER_FUNCTION,
    STRUCTMEMBER_TYPE, // Unused
};

struct StructMemberDef {
    TypeMapString coreTypename; // Typename without template or namespace
    TypeMapString memberName;
    StructMemberType type;
};

#define STRUCT_MEMBERS_MAX 256
struct StructDef {
    TypeMapString name;
    StructMemberDef members[STRUCT_MEMBERS_MAX];
    u32 membersCount;
};


struct TypeMapInstance {
    StructDef* typenames;
    u64 typenamesCount;
    u64 typenamesMax;
};

struct TypeMap {
    TypeMapInstance* instances;
    u64 instancesSize;
};

TypeMap allTypes;

String_Const_u8 toConstU8(TypeMapString& str)
{
    String_Const_u8 ret;
    ret.str = str.str;
    ret.size = str.size;
    return ret;
}

int initFindTypes()
{
    allTypes.instances = (TypeMapInstance*)malloc(sizeof(TypeMapInstance) * 128);
    allTypes.instancesSize = 128;
    memset(allTypes.instances, 0, sizeof(TypeMapInstance) * allTypes.instancesSize);
    return 0;
}

int callInitFindTypes = initFindTypes();

bool isType(String_Const_u8 str);

void insertType(StructDef& def)
{
    String_Const_u8 tnStr;
    tnStr.str = def.name.str;
    tnStr.size = def.name.size;
    if (isType(tnStr))
    {
        return;
    }
    
    unsigned int hashVal = StringCRC32((char*)tnStr.str, (int)tnStr.size);
    TypeMapInstance& inst = allTypes.instances[hashVal % allTypes.instancesSize];
    if (inst.typenames == NULL)
    {
        inst.typenames = (StructDef*)malloc(sizeof(StructDef) * 128);
        inst.typenamesMax = 128;
        inst.typenamesCount = 0;
    }
    if (inst.typenamesCount + 1 >= inst.typenamesMax)
    {
        StructDef* newAr = (StructDef*)malloc(sizeof(StructDef) * inst.typenamesMax * 2);
        inst.typenamesMax *= 2;
        memcpy(newAr, inst.typenames, sizeof(StructDef) * inst.typenamesCount);
        free(inst.typenames);
        inst.typenames = newAr;
    }
    
    inst.typenames[inst.typenamesCount++] = def;
}

bool isType(String_Const_u8 str)
{
    unsigned int hashVal = StringCRC32((char*)str.str, (int)str.size);
    TypeMapInstance& inst = allTypes.instances[hashVal % allTypes.instancesSize];
    
    if (inst.typenames == NULL)
    {
        return false;
    }
    
    for (u64 i = 0; i < inst.typenamesCount; ++i)
    {
        StructDef& tStr = inst.typenames[i];
        String_Const_u8 as4Str;
        as4Str.str = tStr.name.str;
        as4Str.size = tStr.name.size;
        if (string_compare(str, as4Str) == 0)
        {
            return true;
        }
    }
    return false;
}

StructDef* getType(String_Const_u8 name)
{
    unsigned int hashVal = StringCRC32((char*)name.str, (int)name.size);
    TypeMapInstance& inst = allTypes.instances[hashVal % allTypes.instancesSize];
    
    if (inst.typenames == NULL)
    {
        return false;
    }
    
    for (u64 i = 0; i < inst.typenamesCount; ++i)
    {
        StructDef& tStr = inst.typenames[i];
        String_Const_u8 as4Str;
        as4Str.str = tStr.name.str;
        as4Str.size = tStr.name.size;
        if (string_compare(name, as4Str) == 0)
        {
            return &inst.typenames[i];
        }
    }
    return NULL;
}

bool lagParseStruct(Application_Links* app, Buffer_ID buffer, Token_Array tokens, i64 begin, StructDef& ret);

// TODO: Make enum type look green instead of blue
void findTypes(Application_Links* app, Buffer_ID buffer)
{
    Token_Array tokens = get_token_array_from_buffer(app, buffer);
    
    for (i64 i = 0; i < tokens.count; ++i)
    {
        Token& it = tokens.tokens[i];
        
        if (it.kind != TokenBaseKind_Keyword)
        {
            continue;
        }
        
        if (bufferMatchString(app, buffer, it.pos, string_u8_litexpr("enum class")))
        {
            if (i + 4 < tokens.count)
            {
                //API_VTable_custom
                StructDef def;
                def.membersCount = 0;
                //u8 tStr[256];
                Token& next = tokens.tokens[i + 4];
                if (next.kind != TokenBaseKind_Identifier ||
                    next.size >= 256)
                {
                    continue;
                }
                buffer_read_range(app, buffer, { next.pos, next.pos + next.size }, def.name.str);
                def.name.size = next.size;
                insertType(def);
            }
        }
        else if (bufferMatchString(app, buffer, it.pos, string_u8_litexpr("using")) || bufferMatchString(app, buffer, it.pos, string_u8_litexpr("enum")))
        {
            if (i + 2 < tokens.count)
            {
                StructDef def;
                def.membersCount = 0;
                //u8 tStr[256];
                Token& next = tokens.tokens[i + 2];
                if (next.kind != TokenBaseKind_Identifier ||
                    next.size >= 256)
                {
                    continue;
                }
                buffer_read_range(app, buffer, { next.pos, next.pos + next.size }, def.name.str);
                def.name.size = next.size;
                insertType(def);
            }
        }
        else if (bufferMatchString(app, buffer, it.pos, string_u8_litexpr("struct")) || bufferMatchString(app, buffer, it.pos, string_u8_litexpr("class")))
        {
            StructDef def;
            def.membersCount = 0;
            //u8 tStr[256];
            Token& next = tokens.tokens[i + 2];
            if (next.kind != TokenBaseKind_Identifier ||
                next.size >= 256)
            {
                continue;
            }
            buffer_read_range(app, buffer, { next.pos, next.pos + next.size }, def.name.str);
            def.name.size = next.size;
            lagParseStruct(app, buffer, tokens, i, def);
            insertType(def);
        }
    }
}

CUSTOM_COMMAND_SIG(lag_load_types)
CUSTOM_DOC("Load the type information for all buffers.")
{
    for (Buffer_ID bufferIt = get_buffer_next(app, 0, Access_Always); bufferIt != 0; bufferIt = get_buffer_next(app, bufferIt, Access_Always))
    {
        findTypes(app, bufferIt);
    }
}



void seekToTokenKind(Token_Array& tokens, i64& current, u16 kind)
{
    while (current < tokens.count)
    {
        if (tokens.tokens[current].kind == kind)
        {
            return;
        }
        ++current;
    }
    
}
void seekToTokenSubKind(Token_Array& tokens, i64& current, u16 subKind)
{
    while (current < tokens.count)
    {
        if (tokens.tokens[current].sub_kind == subKind)
        {
            return;
        }
        ++current;
    }
}

void skipWhitespace(Token_Array& tokens, i64& current)
{
    while (current < tokens.count)
    {
        if (tokens.tokens[current].kind != TokenBaseKind_Whitespace && tokens.tokens[current].kind != TokenBaseKind_Comment)
        {
            return;
        }
        ++current;
    }
}

void skipWhitespaceBack(Token_Array& tokens, i64& current)
{
    while (current > 0)
    {
        if (tokens.tokens[current].kind != TokenBaseKind_Whitespace && tokens.tokens[current].kind != TokenBaseKind_Comment)
        {
            return;
        }
        --current;
    }
}

void readTokenToString(Application_Links* app, Buffer_ID buffer, Token_Array& tokens, i64 index, TypeMapString& str)
{
    Token& t = tokens.tokens[index];
    if (t.size >= 256)
    {
        return;
    }
    
    Range_i64 r;
    r.min = t.pos;
    r.max = t.pos + t.size;
    str.size = t.size;
    buffer_read_range(app, buffer, r, str.str);
}

// Parses type before variable
bool parseDecl(Application_Links* app, Buffer_ID buffer, Token_Array tokens, i64& i, i64& typenameIdentifier, i64* members, i64& membersCount, i64 membersMax)
{
    membersCount = 0;
    
    // Actual content of the struct on the initial scope
    Token& it = tokens.tokens[i];
    if (it.kind != TokenBaseKind_Identifier && !(it.sub_kind >= TokenCppKind_Void && it.sub_kind <= TokenCppKind_Signed))
    {
        // Error
        i++;
        skipWhitespace(tokens, i);
        return false;
    }
    
    typenameIdentifier = i;
    i++;
    skipWhitespace(tokens, i);
    if (tokens.tokens[i].kind == TokenCppKind_Star || tokens.tokens[i].kind == TokenCppKind_And)
    {
        ++i;
        skipWhitespace(tokens, i);
    }
    
    //if (bufferMatchString(app, buffer, i, string_u8_litexpr("")))
    // TokenCppKind in lexer_cpp.h
    // Declaration of the first member
    i64 memberIdentifier = -1;
    bool inTemplateDecl = false;
    bool hadColonColon = false;
    while (i < tokens.count)
    {
        Token& subIt = tokens.tokens[i];
        
        if (inTemplateDecl)
        {
            if (subIt.kind == TokenBaseKind_Operator && subIt.sub_kind == TokenCppKind_Grtr)
            {
                // Done, get member identifier
                ++i;
                skipWhitespace(tokens, i);
                
                if (tokens.tokens[i].kind == TokenCppKind_Star || tokens.tokens[i].kind == TokenCppKind_And)
                {
                    ++i;
                    skipWhitespace(tokens, i);
                }
                
                if (tokens.tokens[i].kind != TokenBaseKind_Identifier)
                {
                    // Error
                    log_string(app, string_u8_litexpr("Lag parsing error: expected identifier."));
                    break;
                }
                
                memberIdentifier = i;
                if (membersCount < membersMax)
                    members[membersCount++] = i;
                i64 j = i;
                ++j;
                skipWhitespace(tokens, j);
                while (j < tokens.count)
                {
                    if (tokens.tokens[j].sub_kind == TokenCppKind_Comma)
                    {
                        ++j;
                        skipWhitespace(tokens, j);
                        if (j < tokens.count && tokens.tokens[j].kind == TokenBaseKind_Identifier)
                        {
                            if (membersCount < membersMax)
                                members[membersCount++] = j;
                            i = j;
                        }
                        else
                        {
                            break;
                        }
                    }
                    else
                    {
                        break;
                    }
                    ++j;
                    skipWhitespace(tokens, j);
                }
                
                break;
            }
        }
        else
        {
            if (subIt.kind == TokenBaseKind_Identifier)
            {
                if (hadColonColon)
                {
                    typenameIdentifier = i;
                    
                    i64 j = i;
                    ++j;
                    skipWhitespace(tokens, j);
                    if (tokens.tokens[j].kind == TokenCppKind_Star || tokens.tokens[j].kind == TokenCppKind_And)
                    {
                        i = j;
                    }
                    
                    // Next identifier is type name
                    hadColonColon = false;
                }
                else
                {
                    // Found member name
                    memberIdentifier = i;
                    if (membersCount < membersMax)
                        members[membersCount++] = i;
                    i64 j = i;
                    ++j;
                    skipWhitespace(tokens, j);
                    while (j < tokens.count)
                    {
                        if (tokens.tokens[j].sub_kind == TokenCppKind_Comma)
                        {
                            ++j;
                            skipWhitespace(tokens, j);
                            if (j < tokens.count && tokens.tokens[j].kind == TokenBaseKind_Identifier)
                            {
                                if (membersCount < membersMax)
                                    members[membersCount++] = j;
                                i = j;
                            }
                            else
                            {
                                break;
                            }
                        }
                        else
                        {
                            break;
                        }
                        ++j;
                        skipWhitespace(tokens, j);
                    }
                    
                    break;
                }
            }
            else if (subIt.kind == TokenBaseKind_Operator && subIt.sub_kind == TokenCppKind_Less)
            {
                inTemplateDecl = true;
            }
            else if (subIt.kind == TokenBaseKind_Operator && subIt.sub_kind == TokenCppKind_ColonColon)
            {
                // Scope, skip
            }
            else if (subIt.sub_kind == TokenCppKind_ParenOp)
            {
                // This is a function call, probably
                break;
            }
            else
            {
                return false;
            }
        }
        
        i++;
        skipWhitespace(tokens, i);
    }
    
    
    return (memberIdentifier != -1);
}

bool lagParseStruct(Application_Links* app, Buffer_ID buffer, Token_Array tokens, i64 begin, StructDef& ret)
{
    ret.membersCount = 0;
    
    i64 i = begin + 1;
    skipWhitespace(tokens, i);
    // TODO: If the struct has an API macro, take the second identifier as a name instead
    //i64 nameStart = i; //TODO
    
    i++;
    skipWhitespace(tokens, i);
    
    if (tokens.tokens[i].kind != TokenBaseKind_ScopeOpen)
    {
        return false;
    }
    
    i++;
    skipWhitespace(tokens, i);
    //i64 scopeStart = i;
    i64 scopeCounter = 1;
    
    while (i < tokens.count)
    {
        Token it = tokens.tokens[i];
        
        if (it.kind == TokenBaseKind_Comment)
        {
            // Skip comments
        }
        else if (it.kind == TokenBaseKind_ScopeOpen)
        {
            scopeCounter++;
        }
        else if (it.kind == TokenBaseKind_ScopeClose)
        {
            scopeCounter--;
            if (scopeCounter == 0)
            {
                break;
            }
        }
        else if (scopeCounter == 1)
        {
            // Actual content of the struct on the initial scope
            
            // Access modifiers
            {
                while (it.kind == TokenBaseKind_Keyword &&
                       (it.sub_kind == TokenCppKind_Public ||
                        it.sub_kind == TokenCppKind_Private ||
                        it.sub_kind == TokenCppKind_Protected))
                {
                    // Skip keyword and colon
                    ++i;
                    skipWhitespace(tokens, i);
                    ++i;
                    skipWhitespace(tokens, i);
                    it = tokens.tokens[i];
                }
            }
            
            // Prefix keywords
            {
                if (it.kind == TokenBaseKind_Keyword &&
                    (it.sub_kind == TokenCppKind_Virtual ||
                     it.sub_kind == TokenCppKind_Const ||
                     it.sub_kind == TokenCppKind_Volatile ||
                     it.sub_kind == TokenCppKind_Inline))
                {
                    ++i;
                    skipWhitespace(tokens, i);
                    it = tokens.tokens[i];
                }
            }
            
            i64 typenameIdentifier = -1;
            i64 memberIdentifiers[16];
            i64 memberIdentifiersCount = 0;
            bool parseSuccess = parseDecl(app, buffer, tokens, i, typenameIdentifier, memberIdentifiers, memberIdentifiersCount, 16);
            
            // Check for correctness
            if (parseSuccess)
            {
                ++i;
                skipWhitespace(tokens, i);
                
                Token& subIt = tokens.tokens[i];
                if (subIt.kind == TokenBaseKind_ParentheticalOpen)
                {
                    // Function
                    StructMemberDef& member = ret.members[ret.membersCount++];
                    member.type = STRUCTMEMBER_FUNCTION;
                    readTokenToString(app, buffer, tokens, typenameIdentifier, member.coreTypename);
                    readTokenToString(app, buffer, tokens, memberIdentifiers[0], member.memberName);
                    
                    // Search end parentheses
                    while (i < tokens.count)
                    {
                        if (tokens.tokens[i].kind == TokenBaseKind_ParentheticalClose)
                        {
                            ++i;
                            skipWhitespace(tokens, i);
                            
                            if (bufferMatchString(app, buffer, tokens.tokens[i].pos, string_u8_litexpr("override")))
                            {
                                ++i;
                                skipWhitespace(tokens, i);
                            }
                            
                            break;
                        }
                        ++i;
                        skipWhitespace(tokens, i);
                    }
                }
                else //if (subIt.sub_kind == TokenCppKind_Semicolon)
                {
                    // Variable
                    for (i64 m = 0; m < memberIdentifiersCount; ++m)
                    {
                        StructMemberDef& member = ret.members[ret.membersCount++];
                        member.type = STRUCTMEMBER_VARIABLE;
                        readTokenToString(app, buffer, tokens, typenameIdentifier, member.coreTypename);
                        readTokenToString(app, buffer, tokens, memberIdentifiers[m], member.memberName);
                    }
                    
                    //seekToTokenSubKind(tokens, i, TokenCppKind_Semicolon);
                }
            }
        }
        else
        {
            // Skip contents of a function
        }
        
        ++i;
        skipWhitespace(tokens, i);
    }
    
    return true;
}

bool compareString(TypeMapString& str1, TypeMapString& str2)
{
    if (str1.size != str2.size)
        return false;
    
    for (u64 i = 0; i < str1.size; ++i)
    {
        if (str1.str[i] != str2.str[i])
            return false;
    }
    return true;
}

struct AutocompleteInfo {
    bool used = false;
    TypeMapString type;
    i32 selected = 0;
};
AutocompleteInfo autocompleteInfo;

CUSTOM_COMMAND_SIG(lag_autocomplete)
CUSTOM_DOC("Perform autocomplete on the current position.")
{
    View_ID view = get_active_view(app, Access_Always);
    Buffer_ID buffer = view_get_buffer(app, view, Access_Always);
    Token_Array tokens = get_token_array_from_buffer(app, buffer);
    i64 cursorPos = view_get_cursor_pos(app, view);
    i64 i = token_index_from_pos(tokens.tokens, tokens.count, cursorPos);
    
    // Get current stack of access
    TypeMapString accessStack[16];
    i64 accessStackCount = 0;
    {
        --i;
        while (i > 0)
        {
            Token& it = tokens.tokens[i];
            
            if (it.kind == TokenBaseKind_Operator && (it.sub_kind == TokenCppKind_Dot || it.sub_kind == TokenCppKind_Arrow))
            {
                // Do something later?
            }
            else if (it.kind == TokenBaseKind_Identifier)
            {
                // New name
                readTokenToString(app, buffer, tokens, i, accessStack[accessStackCount++]);
            }
            else
            {
                // End of stack
                break;
            }
            
            --i;
        }
    }
    
    if (accessStackCount == 0)
    {
        return;
    }
    
    // Look at code before to find original variable
    // Find semicolon and then look forward
    //i64 scope = 1; // Can be negative since we dont know what stack level we are on
    //i64 maxScope = 1;
    String_Const_u8 accStr;
    accStr.str = accessStack[accessStackCount - 1].str;
    accStr.size = accessStack[accessStackCount - 1].size;
    --i;
    while (i >= 0)
    {
        Token& it = tokens.tokens[i];
        TypeMapString DEBUG_IT_NAME;
        readTokenToString(app, buffer, tokens, i, DEBUG_IT_NAME);
        
        if ((it.kind == TokenBaseKind_StatementClose && it.sub_kind == TokenCppKind_Semicolon) || (it.kind == TokenBaseKind_ScopeClose) || (it.kind == TokenBaseKind_ScopeOpen) ||
            (it.kind == TokenBaseKind_ParentheticalOpen) ||
            (it.sub_kind == TokenCppKind_Comma) ||
            i == 0)
        {
            i64 j = i + 1;
            skipWhitespace(tokens, j);
            readTokenToString(app, buffer, tokens, j, DEBUG_IT_NAME);
            i64 typenameIdentifier = -1;
            i64 memberIdentifiers[16];
            i64 memberIdentifiersCount = 0;
            bool parseSuccess = parseDecl(app, buffer, tokens, j, typenameIdentifier, memberIdentifiers, memberIdentifiersCount, 16);
            if (parseSuccess)
            {
                for (i64 m = 0; m < memberIdentifiersCount; ++m)
                {
                    if (bufferMatchString(app, buffer, tokens.tokens[memberIdentifiers[m]].pos, accStr))
                    {
                        // Found member
                        TypeMapString prnt;
                        readTokenToString(app, buffer, tokens, typenameIdentifier, prnt);
                        String_Const_u8 str;
                        str.str = prnt.str;
                        str.size = prnt.size;
                        log_string(app, str);
                        log_string(app, string_u8_litexpr("\n"));
                        
                        StructDef* type = getType(str);
                        if (type)
                        {
                            autocompleteInfo.used = true;
                            autocompleteInfo.type = prnt;
                            autocompleteInfo.selected = 0;
                            
                            StructDef* tiT = type;
                            for (i64 ti = accessStackCount - 2; ti >= 0; ++ti)
                            {
                                TypeMapString& tiS = accessStack[ti];
                                TypeMapString* outcomeTypename = NULL;
                                for (u32 tj = 0; tj < tiT->membersCount; ++tj)
                                {
                                    if (compareString(tiS, tiT->members[tj].memberName))
                                    {
                                        outcomeTypename = &tiT->members[tj].coreTypename;
                                        break;
                                    }
                                }
                                
                                if (!outcomeTypename)
                                    break;
                                
                                tiT = getType(toConstU8(*outcomeTypename));
                                if (tiT)
                                {
                                    autocompleteInfo.type = *outcomeTypename;
                                }
                                else
                                {
                                    break;
                                }
                            }
                            
                            for (u32 k = 0; k < type->membersCount; ++k)
                            {
                                StructMemberDef& memIt = type->members[k];
                                String_Const_u8 memStr;
                                memStr.str = memIt.memberName.str;
                                memStr.size = memIt.memberName.size;
                                log_string(app, memStr);
                                log_string(app, string_u8_litexpr("\n"));
                            }
                        }
                        
                        return;
                    }
                }
            }
        }
        
        --i;
        skipWhitespaceBack(tokens, i);
    }
}

CUSTOM_COMMAND_SIG(lag_autocomplete_move_down)
CUSTOM_DOC("Move autocomplete cursor down.")
{
    autocompleteInfo.selected += 1;
}

CUSTOM_COMMAND_SIG(lag_autocomplete_move_up)
CUSTOM_DOC("Move autocomplete cursor up.")
{
    autocompleteInfo.selected -= 1;
}

CUSTOM_COMMAND_SIG(lag_autocomplete_close)
CUSTOM_DOC("Close the autocomplete window.")
{
    autocompleteInfo.used = false;
}

CUSTOM_COMMAND_SIG(lag_autocomplete_write)
CUSTOM_DOC("Write selected autocomplete word.")
{
    StructDef* type = getType(toConstU8(autocompleteInfo.type));
    if (!type)
        return;
    
    if (autocompleteInfo.selected < 256)
    {
        View_ID view = get_active_view(app, Access_Always);
        Buffer_ID buffer = view_get_buffer(app, view, Access_Always);
        i64 cursor = view_get_cursor_pos(app, view);
        TypeMapString& name = type->members[autocompleteInfo.selected].memberName;
        buffer_replace_range(app, buffer, Ii64(cursor), toConstU8(name));
        Buffer_Seek s;
        s.type = buffer_seek_pos;
        s.pos = cursor + name.size;
        view_set_cursor(app, view, s);
        autocompleteInfo.used = false;
        autocompleteInfo.selected = 0;
        //auto_indent_buffer(app);
    }
}

void lagRenderAutocomplete(Application_Links* app, View_ID view, Face_ID face, Buffer_ID buffer, Frame_Info frameInfo)
{
    Scratch_Block scratch(app);
    
    if (autocompleteInfo.used)
    {
        StructDef* type = getType(toConstU8(autocompleteInfo.type));
        if (!type)
            return;
        
        Rect_f32 rect;
        rect.x0 = global_last_cursor_rect.x0 + 10.f;
        rect.y0 = global_last_cursor_rect.y0 + 16.f;
        rect.x1 = rect.x0 + 200;
        rect.y1 = rect.y0 + 16.f + 16 * type->membersCount;
        
        draw_rectangle(app, rect, 0.f, 0xffffffff);
        
        Vec2_f32 pos;
        pos.x = rect.x0 + 8.f;
        pos.y = rect.y0 + 8.f;
        
        for (u32 i = 0; i < type->membersCount; ++i)
        {
            if (i == (u32)autocompleteInfo.selected)
            {
                Rect_f32 selR;
                selR.x0 = rect.x0;
                selR.y0 = pos.y;
                selR.x1 = rect.x1;
                selR.y1 = selR.y0 + 16.f;
                draw_rectangle(app, selR, 0.f, 0xff888888);
            }
            
            String_Const_u8 tStr = toConstU8(type->members[i].memberName);
            draw_string(app, face, tStr, pos, 0xff000000);
            pos.y += 16.f;
        }
    }
}














