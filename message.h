struct Message {
    uint32_t type;          // indicating message type: VIEW_CHANGE or VC_PROOF
    char *data;             // pointer to message data
    struct Message *next;   // pointer to next struct Message
};

struct Ack {
    uint32_t type;          // indicating message type acked
    uint32_t view_id;       // indicating view_id (attempted or installed)
};

struct View_Change {
    uint32_t type;          // must equal to 2
    uint32_t server_id;     // unique id of the sending server
    uint32_t attempted;     // view number this server is trying to install
};

struct VC_Proof {
    uint32_t type;          // must equal to 3
    uint32_t server_id;     // unique id of the sending server
    uint32_t installed;     // last view number this server installed
};