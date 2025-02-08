enum
{
    PrErr_Truncated = (1 << 0),
};

typedef struct
{
    U8 *buf;
    U64 used;
    U64 cap;
    U32 err; // uses AsText_Error enum flags
} Printer;

static void Pr_Reset(Printer *p)
{
    p->used = 0;
    p->err = 0;
}

static U64 Pr_FreeCount(Printer *p)
{
    if (p->cap >= p->used)
        return p->cap - p->used;
    return 0;
}

static char *Pr_Cstr(Printer *p)
{
    if (!p->cap)
        return "";

    if (Pr_FreeCount(p))
    {
        p->buf[p->used] = 0;
        p->used += 1;
    }
    else
    {
        p->err |= PrErr_Truncated;
        p->buf[p->used-1] = 0;
    }
    return (char *)p->buf;
}

static S8 Pr_S8(Printer *p)
{
    return S8_Make(p->buf, p->used);
}

static void Pr_Add(Printer *p, S8 str)
{
    U64 free_count = Pr_FreeCount(p);
    if (free_count < str.size)
    {
        p->err |= PrErr_Truncated;
        if (!free_count)
            return;

        str = S8_Prefix(str, free_count);
    }

    memcpy(p->buf + p->used, str.str, str.size);
    p->used += str.size;
}

static void Pr_AddCstr(Printer *p, const char *cstr)
{
    Pr_Add(p, S8_ScanCstr(cstr));
}

static void Pr_AddPrinter(Printer *p, Printer *copy)
{
    Pr_Add(p, Pr_S8(copy));
}


//
// Printer Makers
//
static Printer Pr_Make(U8 *buffer, U64 capacity)
{
    Printer res = {.buf = buffer, .cap = capacity};
    return res;
}

static Printer Pr_Alloc(Arena *a, U64 capacity)
{
    Printer res = Pr_Make(Alloc(a, U8, capacity), capacity);
    return res;
}

#define Pr_MakeOnStack(PrinterName, Size) \
U8 PrinterName##_buffer[Size]; \
Printer PrinterName = Pr_Make(PrinterName##_buffer, Size)
