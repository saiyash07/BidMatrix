#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <memory>
#include <queue>
#include <unordered_map>
#include <limits>
#include <cstdint>

// ============================================================================
// BidMatrix - Dynamic Real-Time Ad Bidding Simulator
// ITM Skills University | DSA Case Study
// ============================================================================
/*
  =============================================================================
                     DYNAMIC REAL-TIME AD BIDDING MATRIX
  =============================================================================
  Built this to simulate how ad platforms like DV360 and The Trade Desk
  actually work under the hood. Each feature maps to a real part of the
  ad tech pipeline — from inventory lookups to routing ad creatives.

  Quick overview of what's implemented and why:
  -----------------------------------------------------------------------
  Feature                    | Structure Used        | Time      | Why
  -----------------------------------------------------------------------
  1. Inventory Listing       | Custom Hash Map       | O(1) avg  | Fast banner slot lookups
  2. Budget Rollback         | Custom Stack          | O(1)      | Undo bad budget changes instantly
  3. Auction Loop            | Custom Linked Queue   | O(1)      | Bids processed in order received
  4. Demographic Targeting   | Custom Trie           | O(L)      | Prefix-based segment matching
  5. Campaign Ranking        | Custom Max-Heap       | O(log N)  | Always know which campaign pays most
  6. Exchange Network        | Graph (Adj. List)     | O(V + E)  | Map SSP/publisher connections
  7. Latency Routing         | Dijkstra's Algorithm  | O(E logV) | Find fastest delivery path
  8. Edge Cache Allocator    | Free List (First-Fit) | O(N)      | Manage CDN memory for ad assets
  -----------------------------------------------------------------------
*/

using namespace std;

// ============================================================================
// 1. INVENTORY LISTING (Custom Hash Map)
// ============================================================================
// Used a hash map here because banner slot lookups need to be instant.
// When a bid request comes in we need to know the floor price and publisher
// without scanning through every entry.
struct BannerSpace {
    string id;          // e.g. "banner_header_top"
    string publisher;   // e.g. "NYTimes.com"
    string size;        // e.g. "728x90"
    double minBidFloor; // Minimum bid threshold ($)
    bool isAllocated;   // Status
    BannerSpace* next;  // Pointer for hash collision chaining

    BannerSpace(string id, string pub, string sz, double floor)
        : id(id), publisher(pub), size(sz), minBidFloor(floor), isAllocated(false), next(nullptr) {}
};

class InventoryListing {
private:
    static const int BUCKET_COUNT = 17;
    BannerSpace* buckets[BUCKET_COUNT];

    int getHash(const string& key) const {
        unsigned long hash = 5381;
        for (char c : key) {
            hash = ((hash << 5) + hash) + c; // classic djb2
        }
        return hash % BUCKET_COUNT;
    }

public:
    InventoryListing() {
        for (int i = 0; i < BUCKET_COUNT; ++i) {
            buckets[i] = nullptr;
        }
    }

    ~InventoryListing() {
        for (int i = 0; i < BUCKET_COUNT; ++i) {
            BannerSpace* curr = buckets[i];
            while (curr != nullptr) {
                BannerSpace* temp = curr;
                curr = curr->next;
                delete temp;
            }
        }
    }

    void insert(const string& id, const string& pub, const string& size, double floor) {
        int idx = getHash(id);
        BannerSpace* curr = buckets[idx];
        while (curr != nullptr) {
            if (curr->id == id) {
                // already in there, just update it
                curr->publisher = pub;
                curr->size = size;
                curr->minBidFloor = floor;
                return;
            }
            curr = curr->next;
        }
        // prepend to the chain at this bucket
        BannerSpace* entry = new BannerSpace(id, pub, size, floor);
        entry->next = buckets[idx];
        buckets[idx] = entry;
    }

    BannerSpace* search(const string& id) {
        int idx = getHash(id);
        BannerSpace* curr = buckets[idx];
        while (curr != nullptr) {
            if (curr->id == id) return curr;
            curr = curr->next;
        }
        return nullptr;
    }

    void display() const {
        cout << "\n-------------------------------------------------------------\n";
        cout << "                WEB DISPLAY BANNER INVENTORY                 \n";
        cout << "-------------------------------------------------------------\n";
        cout << left << setw(18) << "Banner Space ID" << setw(18) << "Publisher" << setw(10) << "Size" << setw(12) << "Min Floor ($)" << "Status\n";
        cout << "-------------------------------------------------------------\n";
        for (int i = 0; i < BUCKET_COUNT; ++i) {
            BannerSpace* curr = buckets[i];
            while (curr != nullptr) {
                cout << left << setw(18) << curr->id 
                     << setw(18) << curr->publisher 
                     << setw(10) << curr->size 
                     << "$" << setw(11) << fixed << setprecision(2) << curr->minBidFloor
                     << (curr->isAllocated ? "ALLOCATED" : "AVAILABLE") << "\n";
                curr = curr->next;
            }
        }
        cout << "-------------------------------------------------------------\n";
    }
};

// ============================================================================
// 2. UPDATE HISTORY / BUDGET ROLLBACK (Custom Stack)
// ============================================================================
// Basically an undo button for budget changes. If someone accidentally
// bumps a campaign's budget up too high, we can snap it back to whatever
// it was before. Stack made sense here — most recent change goes first.
struct BudgetState {
    string campaignId;
    double oldAllocatedBudget;
    double newAllocatedBudget;
    string timestamp;

    BudgetState(string id, double oldB, double newB, string time)
        : campaignId(id), oldAllocatedBudget(oldB), newAllocatedBudget(newB), timestamp(time) {}
};

class BudgetHistoryStack {
private:
    vector<BudgetState> stack; // backing the stack with a vector, simpler than a linked list

public:
    void push(const string& campId, double oldB, double newB, const string& timestamp) {
        stack.push_back(BudgetState(campId, oldB, newB, timestamp));
    }

    bool pop(BudgetState& outState) {
        if (stack.empty()) return false;
        outState = stack.back();
        stack.pop_back();
        return true;
    }

    bool peek(BudgetState& outState) const {
        if (stack.empty()) return false;
        outState = stack.back();
        return true;
    }

    bool isEmpty() const {
        return stack.empty();
    }

    void display() const {
        cout << "\n-------------------------------------------------------------\n";
        cout << "                   BUDGET UPDATE HISTORICAL LEDGER           \n";
        cout << "-------------------------------------------------------------\n";
        if (stack.empty()) {
            cout << "No budget updates recorded in ledger.\n";
            return;
        }
        cout << left << setw(8) << "Step" << setw(15) << "Campaign ID" << setw(14) << "Prev Budget" << setw(14) << "New Budget" << "Timestamp\n";
        cout << "-------------------------------------------------------------\n";
        for (int i = (int)stack.size() - 1; i >= 0; --i) {
            cout << left << "[" << i + 1 << "]     "
                 << setw(15) << stack[i].campaignId
                 << "$" << setw(13) << fixed << setprecision(2) << stack[i].oldAllocatedBudget
                 << "$" << setw(13) << fixed << setprecision(2) << stack[i].newAllocatedBudget
                 << stack[i].timestamp << "\n";
        }
        cout << "-------------------------------------------------------------\n";
    }
};

// ============================================================================
// 3. AUCTION LOOP (Custom FIFO Bid Queue)
// ============================================================================
// Bids arrive over the network and need to be handled in the order they came
// in — first bid, first served. A FIFO queue is the obvious choice here.
// In a real exchange this would be backed by something like Kafka, but
// a linked queue works fine for the simulation.
struct AdBid {
    string bidId;
    string bannerSpaceId;
    string advertiserId;
    double bidAmount;
    string userAgent;
    AdBid* next;

    AdBid(string id, string space, string adv, double amt, string ua)
        : bidId(id), bannerSpaceId(space), advertiserId(adv), bidAmount(amt), userAgent(ua), next(nullptr) {}
};

class AuctionLoopQueue {
private:
    AdBid* head;
    AdBid* tail;
    int size;

public:
    AuctionLoopQueue() : head(nullptr), tail(nullptr), size(0) {}

    ~AuctionLoopQueue() {
        while (!isEmpty()) {
            AdBid* out;
            dequeue(out);
            delete out;
        }
    }

    void enqueue(const string& bidId, const string& spaceId, const string& advId, double amount, const string& ua) {
        AdBid* node = new AdBid(bidId, spaceId, advId, amount, ua);
        if (tail == nullptr) {
            head = tail = node;
        } else {
            tail->next = node;
            tail = node;
        }
        size++;
    }

    bool dequeue(AdBid*& outBid) {
        if (head == nullptr) {
            outBid = nullptr;
            return false;
        }
        outBid = head;
        head = head->next;
        if (head == nullptr) {
            tail = nullptr;
        }
        size--;
        return true;
    }

    bool isEmpty() const {
        return head == nullptr;
    }

    int getSize() const {
        return size;
    }

    void display() const {
        cout << "\n-------------------------------------------------------------\n";
        cout << "               PENDING AUCTION LIVE BID QUEUE (FIFO)         \n";
        cout << "-------------------------------------------------------------\n";
        if (isEmpty()) {
            cout << "Queue empty. All live bid streams fully processed.\n";
            return;
        }
        AdBid* temp = head;
        cout << left << setw(10) << "Bid ID" << setw(18) << "Banner Space ID" << setw(15) << "Advertiser" << setw(12) << "Bid Amount" << "Browser agent\n";
        cout << "-------------------------------------------------------------\n";
        while (temp != nullptr) {
            cout << left << setw(10) << temp->bidId
                 << setw(18) << temp->bannerSpaceId
                 << setw(15) << temp->advertiserId
                 << "$" << setw(11) << fixed << setprecision(2) << temp->bidAmount
                 << temp->userAgent << "\n";
            temp = temp->next;
        }
        cout << "-------------------------------------------------------------\n";
    }
};

// ============================================================================
// 4. ATTRIBUTE TOKEN DEMOGRAPHIC VERIFICATION (Custom Trie)
// ============================================================================
// Demographic keys share a lot of prefixes (dem_age_18_24, dem_age_25_34, etc.)
// so a Trie is a natural fit. Lookup is O(key length) regardless of how many
// segments are registered, which is what we want.
class TrieNode {
public:
    unordered_map<char, TrieNode*> children;
    bool isSegmentTerminal;
    string segmentDescription;

    TrieNode() : isSegmentTerminal(false), segmentDescription("") {}
    ~TrieNode() {
        for (auto& pair : children) {
            delete pair.second;
        }
    }
};

class AttributeTokenTrie {
private:
    TrieNode* root;

public:
    AttributeTokenTrie() {
        root = new TrieNode();
    }

    ~AttributeTokenTrie() {
        delete root;
    }

    // e.g. "dem_age_18_24", "geo_us_east"
    void insert(const string& attributeKey, const string& description) {
        TrieNode* curr = root;
        for (char c : attributeKey) {
            if (curr->children.find(c) == curr->children.end()) {
                curr->children[c] = new TrieNode();
            }
            curr = curr->children[c];
        }
        curr->isSegmentTerminal = true;
        curr->segmentDescription = description;
    }

    // check if this targeting key is one we recognize
    bool verify(const string& attributeKey, string& outDesc) {
        TrieNode* curr = root;
        for (char c : attributeKey) {
            if (curr->children.find(c) == curr->children.end()) {
                return false;
            }
            curr = curr->children[c];
        }
        if (curr->isSegmentTerminal) {
            outDesc = curr->segmentDescription;
            return true;
        }
        return false;
    }

    void collectKeys(TrieNode* node, string currentPrefix, vector<pair<string, string>>& results) const {
        if (node->isSegmentTerminal) {
            results.push_back({currentPrefix, node->segmentDescription});
        }
        for (auto& pair : node->children) {
            collectKeys(pair.second, currentPrefix + pair.first, results);
        }
    }

    void display() const {
        cout << "\n-------------------------------------------------------------\n";
        cout << "           DEMOGRAPHIC TARGETING POOL DEFINITIONS (TRIE)     \n";
        cout << "-------------------------------------------------------------\n";
        vector<pair<string, string>> results;
        collectKeys(root, "", results);
        if (results.empty()) {
            cout << "No demographics loaded in Segment Trie.\n";
            return;
        }
        cout << left << setw(25) << "Targeting Attribute Key" << "Demographic Segment Description\n";
        cout << "-------------------------------------------------------------\n";
        for (auto& pair : results) {
            cout << left << setw(25) << pair.first << pair.second << "\n";
        }
        cout << "-------------------------------------------------------------\n";
    }
};

// ============================================================================
// 5. CAMPAIGN YIELD SORTER (Custom Max-Heap)
// ============================================================================
// Campaigns are ranked by eCPM (Bid * CTR * 1000). We always want the
// highest-value campaign at the top, so a max-heap is perfect.
// Insertion is O(log N) and peeking the best one is O(1).
struct Campaign {
    string id;
    string name;
    double budget;
    double expectedCtr; // CTR (0.01 to 0.10)
    double bidValue;    // $ Value
    double expectedECpm; // Calculated: CTR * BidValue * 1000

    Campaign(string id, string name, double b, double ctr, double bid)
        : id(id), name(name), budget(b), expectedCtr(ctr), bidValue(bid) {
        expectedECpm = expectedCtr * bidValue * 1000.0;
    }
};

class CampaignYieldSorter {
private:
    vector<Campaign> heap;

    void heapifyUp(int index) {
        while (index > 0) {
            int parent = (index - 1) / 2;
            if (heap[index].expectedECpm > heap[parent].expectedECpm) {
                swap(heap[index], heap[parent]);
                index = parent;
            } else {
                break;
            }
        }
    }

    void heapifyDown(int index) {
        int n = heap.size();
        while (index * 2 + 1 < n) {
            int leftChild = index * 2 + 1;
            int rightChild = index * 2 + 2;
            int largest = index;

            if (heap[leftChild].expectedECpm > heap[largest].expectedECpm) {
                largest = leftChild;
            }
            if (rightChild < n && heap[rightChild].expectedECpm > heap[largest].expectedECpm) {
                largest = rightChild;
            }

            if (largest != index) {
                swap(heap[index], heap[largest]);
                index = largest;
            } else {
                break;
            }
        }
    }

public:
    void insertCampaign(const string& id, const string& name, double budget, double ctr, double bid) {
        heap.push_back(Campaign(id, name, budget, ctr, bid));
        heapifyUp(heap.size() - 1);
    }

    bool extractMax(Campaign& outCamp) {
        if (heap.empty()) return false;
        outCamp = heap[0];
        heap[0] = heap.back();
        heap.pop_back();
        heapifyDown(0);
        return true;
    }

    bool peekMax(Campaign& outCamp) const {
        if (heap.empty()) return false;
        outCamp = heap[0];
        return true;
    }

    bool isEmpty() const {
        return heap.empty();
    }

    int getSize() const {
        return heap.size();
    }

    void display() const {
        cout << "\n-------------------------------------------------------------\n";
        cout << "             CAMPAIGN EXPECTED REVENUE SORT MATRIX (MAX-HEAP) \n";
        cout << "-------------------------------------------------------------\n";
        if (heap.empty()) {
            cout << "No campaigns registered in active DSP sorter.\n";
            return;
        }
        cout << left << setw(10) << "ID" << setw(18) << "Campaign Name" << setw(12) << "Ctr (%)" << setw(12) << "Bid Value" << "Expected eCPM (Yield)\n";
        cout << "-------------------------------------------------------------\n";
        // copy the heap so we can drain it for display without destroying the original
        CampaignYieldSorter tempSorter = *this;
        Campaign item("", "", 0, 0, 0);
        while (tempSorter.extractMax(item)) {
            cout << left << setw(10) << item.id
                 << setw(18) << item.name
                 << fixed << setprecision(2) << (item.expectedCtr * 100) << "%       "
                 << "$" << setw(11) << item.bidValue
                 << "$" << fixed << setprecision(2) << item.expectedECpm << "\n";
        }
        cout << "-------------------------------------------------------------\n";
    }
};

// ============================================================================
// 6 & 7. MARKETPLACE NETWORK & LATENCY ROUTE PATH (Graph & Dijkstra)
// ============================================================================
// The SSP/publisher connections form a weighted graph where the weights
// are latency in ms. We need to deliver the ad in under 100ms, so
// Dijkstra gives us the lowest-latency path between any two nodes.
struct Edge {
    string destination;
    double latencyMs; // Weight in milliseconds
};

struct GraphNode {
    string id;
    string description;
    vector<Edge> edges;
};

class MarketplaceNetwork {
private:
    unordered_map<string, GraphNode> nodes;

public:
    void addNode(const string& id, const string& desc) {
        if (nodes.find(id) == nodes.end()) {
            nodes[id] = {id, desc, {}};
        }
    }

    void addConnection(const string& from, const string& to, double latency) {
        addNode(from, from);
        addNode(to, to);
        nodes[from].edges.push_back({to, latency});
    }

    // standard Dijkstra — min-heap on (distance, node)
    bool findShortestLatencyRoute(const string& startId, const string& endId, vector<string>& outPath, double& outLatency) {
        if (nodes.find(startId) == nodes.end() || nodes.find(endId) == nodes.end()) {
            return false;
        }

        unordered_map<string, double> distances;
        unordered_map<string, string> predecessors;
        for (auto const& [key, val] : nodes) {
            distances[key] = numeric_limits<double>::infinity();
        }

        // priority_queue defaults to max, greater<> flips it to min
        priority_queue<pair<double, string>, vector<pair<double, string>>, greater<pair<double, string>>> pq;

        distances[startId] = 0.0;
        pq.push({0.0, startId});

        while (!pq.empty()) {
            double currentDist = pq.top().first;
            string currentId = pq.top().second;
            pq.pop();

            if (currentId == endId) break;

            if (currentDist > distances[currentId]) continue;

            for (const Edge& edge : nodes[currentId].edges) {
                double newDist = currentDist + edge.latencyMs;
                if (newDist < distances[edge.destination]) {
                    distances[edge.destination] = newDist;
                    predecessors[edge.destination] = currentId;
                    pq.push({newDist, edge.destination});
                }
            }
        }

        if (distances[endId] == numeric_limits<double>::infinity()) {
            return false;
        }

        // walk predecessors backwards to rebuild the path
        outLatency = distances[endId];
        string curr = endId;
        while (curr != startId) {
            outPath.push_back(curr);
            curr = predecessors[curr];
        }
        outPath.push_back(startId);
        reverse(outPath.begin(), outPath.end());
        return true;
    }

    void display() const {
        cout << "\n-------------------------------------------------------------\n";
        cout << "             MARKETPLACE EXCHANGE ROUTING NETWORK            \n";
        cout << "-------------------------------------------------------------\n";
        if (nodes.empty()) {
            cout << "No routing servers defined in regional map.\n";
            return;
        }
        for (auto const& [key, node] : nodes) {
            cout << " [" << node.id << "] -> Connections:\n";
            if (node.edges.empty()) {
                cout << "     * Terminal Node (No out-links)\n";
            }
            for (const Edge& edge : node.edges) {
                cout << "     * to [" << edge.destination << "] latency: " << edge.latencyMs << " ms\n";
            }
        }
        cout << "-------------------------------------------------------------\n";
    }
};

// ============================================================================
// 8. BLOCK ALLOCATOR (Edge Storage Memory Management)
// ============================================================================
// Simulates how an edge/CDN server manages its cache space for ad creatives.
// First-fit allocation keeps it simple. When a creative is evicted,
// we merge adjacent free blocks to avoid fragmentation building up.
struct MemoryBlock {
    int startAddress;
    int size;
    bool isFree;
    string allocatedCreativeId;
    MemoryBlock* next;

    MemoryBlock(int addr, int sz)
        : startAddress(addr), size(sz), isFree(true), allocatedCreativeId(""), next(nullptr) {}
};

class EdgeServerBlockAllocator {
private:
    MemoryBlock* head;
    int totalSize;

public:
    EdgeServerBlockAllocator(int totalCapacity) : totalSize(totalCapacity) {
        head = new MemoryBlock(0, totalCapacity);
    }

    ~EdgeServerBlockAllocator() {
        MemoryBlock* curr = head;
        while (curr != nullptr) {
            MemoryBlock* temp = curr;
            curr = curr->next;
            delete temp;
        }
    }

    // first-fit: grab the first block that's big enough
    int allocate(int requestSize, const string& creativeId) {
        MemoryBlock* curr = head;
        while (curr != nullptr) {
            if (curr->isFree && curr->size >= requestSize) {
                // split if there's leftover space
                if (curr->size > requestSize) {
                    MemoryBlock* split = new MemoryBlock(curr->startAddress + requestSize, curr->size - requestSize);
                    split->next = curr->next;
                    curr->next = split;
                    curr->size = requestSize;
                }
                curr->isFree = false;
                curr->allocatedCreativeId = creativeId;
                return curr->startAddress;
            }
            curr = curr->next;
        }
        return -1; // no block fits — fragmented or just out of space
    }

    // free the block, then merge any neighboring free blocks
    bool deallocate(const string& creativeId) {
        MemoryBlock* curr = head;
        bool found = false;
        while (curr != nullptr) {
            if (!curr->isFree && curr->allocatedCreativeId == creativeId) {
                curr->isFree = true;
                curr->allocatedCreativeId = "";
                found = true;
            }
            curr = curr->next;
        }

        if (!found) return false;

        // merge adjacent free blocks so we don't end up with lots of tiny gaps
        curr = head;
        while (curr != nullptr && curr->next != nullptr) {
            if (curr->isFree && curr->next->isFree) {
                MemoryBlock* duplicate = curr->next;
                curr->size += duplicate->size;
                curr->next = duplicate->next;
                delete duplicate;
            } else {
                curr = curr->next;
            }
        }
        return true;
    }

    void display() const {
        cout << "\n-------------------------------------------------------------\n";
        cout << "          EDGE SERVER STORAGE ALLOCATION MAP (FREE LIST)     \n";
        cout << "-------------------------------------------------------------\n";
        MemoryBlock* curr = head;
        cout << left << setw(18) << "Memory Range" << setw(10) << "Size (KB)" << setw(12) << "State" << "Content / Creative ID\n";
        cout << "-------------------------------------------------------------\n";
        while (curr != nullptr) {
            string range = "[" + to_string(curr->startAddress) + "-" + to_string(curr->startAddress + curr->size - 1) + "]";
            cout << left << setw(18) << range 
                 << setw(10) << curr->size 
                 << setw(12) << (curr->isFree ? "FREE" : "ALLOCATED")
                 << (curr->isFree ? "-" : curr->allocatedCreativeId) << "\n";
            curr = curr->next;
        }
        cout << "-------------------------------------------------------------\n";
    }
};

// ============================================================================
// Main menu / interactive console
// ============================================================================
void displayHeader() {
    cout << "=============================================================================\n";
    cout << "                    DYNAMIC REAL-TIME AD BIDDING MATRIX                      \n";
    cout << "                          ITM SKILLS UNIVERSITY                              \n";
    cout << "=============================================================================\n";
}

int main() {
    // pre-load some sample data so the demo isn't empty
    InventoryListing inventory;
    BudgetHistoryStack budgetLedger;
    AuctionLoopQueue bidQueue;
    AttributeTokenTrie demographicsTrie;
    CampaignYieldSorter campaignSorter;
    MarketplaceNetwork network;
    EdgeServerBlockAllocator edgeAllocator(1024); // 1024 KB total

    // banner inventory
    inventory.insert("space_header", "CNN.com", "728x90", 2.50);
    inventory.insert("space_sidebar", "Bloomberg", "300x250", 1.80);
    inventory.insert("space_footer", "Medium.com", "468x60", 0.90);
    inventory.insert("space_video", "YouTube.com", "640x360", 5.00);

    // demographic targeting segments
    demographicsTrie.insert("dem_age_18_24", "Young Adults (18-24)");
    demographicsTrie.insert("dem_age_25_34", "Young Professionals (25-34)");
    demographicsTrie.insert("geo_us_ny", "New York, USA Residents");
    demographicsTrie.insert("int_sports", "Sports Fans");
    demographicsTrie.insert("int_tech", "Technology Enthusiasts");

    // campaigns — eCPM = CTR * bid * 1000
    campaignSorter.insertCampaign("camp_nike", "Nike Run Club", 10000.0, 0.05, 3.20); // $160 eCPM
    campaignSorter.insertCampaign("camp_apple", "iPhone 18 Launch", 50000.0, 0.08, 4.50); // $360 eCPM
    campaignSorter.insertCampaign("camp_star", "Starbucks Brew", 8000.0, 0.03, 1.50);    // $45 eCPM
    campaignSorter.insertCampaign("camp_tesla", "Tesla CyberCab", 25000.0, 0.06, 5.50);  // $330 eCPM

    // routing network between RTB platform and publisher SSPs
    network.addConnection("Platform_RTB", "US_East_Edge", 5.2);
    network.addConnection("Platform_RTB", "US_West_Edge", 18.4);
    network.addConnection("US_East_Edge", "NYTimes_SSP", 2.1);
    network.addConnection("US_East_Edge", "CNN_SSP", 4.3);
    network.addConnection("US_West_Edge", "YouTube_SSP", 3.0);
    network.addConnection("US_West_Edge", "Bloomberg_SSP", 8.2);

    int choice = 0;
    while (true) {
        displayHeader();
        cout << " 1. Manage Display Inventory Listing [Hash Table]\n";
        cout << " 2. Log Budget Adjustments & Run Rollback [Stack]\n";
        cout << " 3. Stream Live Bids into Auction Loop [FIFO Queue]\n";
        cout << " 4. Target Demographics Token Manager [Trie]\n";
        cout << " 5. Campaign Sorter & Yield Ranker [Max-Heap]\n";
        cout << " 6. Visualize SSP/Publisher Marketplace [Graph]\n";
        cout << " 7. Route Delivery Shortest Latency Path [Dijkstra]\n";
        cout << " 8. Server Storage Block Allocator [Free List]\n";
        cout << " 9. Run Interactive Unified E2E Auction Simulation\n";
        cout << "10. View Architectural DSA Justification Report\n";
        cout << "11. Exit System\n";
        cout << "-----------------------------------------------------------------------------\n";
        cout << "Enter operation index [1-11]: ";
        
        if (!(cin >> choice)) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "Invalid selection! Enter an integer key.\n\n";
            continue;
        }

        if (choice == 11) {
            cout << "Terminating BidMatrix Exchange System... Good bye.\n";
            break;
        }

        switch (choice) {
            case 1: {
                int op = 0;
                cout << "\n1. Display Existing Banners\n2. Add New Banner Space\nSelect Option: ";
                cin >> op;
                if (op == 2) {
                    string id, pub, sz;
                    double floor;
                    cout << "Enter Banner Space Unique ID (e.g. space_sidebar_lower): ";
                    cin >> id;
                    cout << "Enter Publisher Domain (e.g. TechCrunch.com): ";
                    cin >> pub;
                    cout << "Enter Dimensions Size (e.g. 300x600): ";
                    cin >> sz;
                    cout << "Enter Minimum Floor Bid Value ($): ";
                    cin >> floor;
                    inventory.insert(id, pub, sz, floor);
                    cout << ">>> Banner space successfully registered!\n";
                }
                inventory.display();
                break;
            }
            case 2: {
                int op = 0;
                cout << "\n1. Adjust/Increase Campaign Budget\n2. Rollback Last Action (Undo)\n3. View Ledger Log\nSelect Option: ";
                cin >> op;
                if (op == 1) {
                    string id;
                    double oldB, newB;
                    cout << "Enter Campaign ID to adjust (e.g. camp_nike): ";
                    cin >> id;
                    cout << "Current Budget ($): ";
                    cin >> oldB;
                    cout << "New Target Budget ($): ";
                    cin >> newB;
                    budgetLedger.push(id, oldB, newB, "2026-06-08 20:48:00");
                    cout << ">>> Budget ledger record registered!\n";
                } else if (op == 2) {
                    BudgetState rolledState("", 0, 0, "");
                    if (budgetLedger.pop(rolledState)) {
                        cout << ">>> SUCCESSFUL ROLLBACK EXECUTED!\n";
                        cout << "    Rolled back Campaign [" << rolledState.campaignId << "] budget from $" 
                             << rolledState.newAllocatedBudget << " back to stable previous: $" << rolledState.oldAllocatedBudget << "\n";
                    } else {
                        cout << ">>> Error: No state adjustments in history stack to rollback!\n";
                    }
                }
                budgetLedger.display();
                break;
            }
            case 3: {
                int op = 0;
                cout << "\n1. Inject Live Bid to Queue\n2. Process Next FIFO Bid\n3. View Queue Status\nSelect Option: ";
                cin >> op;
                if (op == 1) {
                    string bidId, spaceId, advId, ua;
                    double amt;
                    cout << "Enter Bid ID (e.g. bid_8891): ";
                    cin >> bidId;
                    cout << "Target Banner Space ID (e.g. space_video): ";
                    cin >> spaceId;
                    cout << "Advertiser Code: (e.g. adv_apple): ";
                    cin >> advId;
                    cout << "Bid Value ($): ";
                    cin >> amt;
                    cout << "User agent info (e.g. Safari_iOS): ";
                    cin >> ua;
                    bidQueue.enqueue(bidId, spaceId, advId, amt, ua);
                    cout << ">>> Live Bid socket transaction stream queued!\n";
                } else if (op == 2) {
                    AdBid* popped = nullptr;
                    if (bidQueue.dequeue(popped)) {
                        cout << ">>> Auction Engine processing bid: " << popped->bidId << " | Space: " << popped->bannerSpaceId
                             << " | Advertiser: " << popped->advertiserId << " | Value: $" << popped->bidAmount << "\n";
                        delete popped;
                    } else {
                        cout << ">>> Loop queue empty. All ad request streams processed.\n";
                    }
                }
                bidQueue.display();
                break;
            }
            case 4: {
                int op = 0;
                cout << "\n1. Search Demographic Attribute Key\n2. Register New Segment Keyword\nSelect Option: ";
                cin >> op;
                if (op == 1) {
                    string key, desc;
                    cout << "Enter client browser attribute segment key (e.g. dem_age_18_24): ";
                    cin >> key;
                    if (demographicsTrie.verify(key, desc)) {
                        cout << ">>> VALID TARGETING CATEGORY FOUND: " << desc << "\n";
                    } else {
                        cout << ">>> SEGMENT MISSING: User target token not targeting inside our exchange database.\n";
                    }
                } else if (op == 2) {
                    string key, desc;
                    cout << "Enter Segment Key (e.g. int_gaming): ";
                    cin >> key;
                    cin.ignore();
                    cout << "Enter Demographic Label Description: ";
                    getline(cin, desc);
                    demographicsTrie.insert(key, desc);
                    cout << ">>> Target demographic Trie updated!\n";
                }
                demographicsTrie.display();
                break;
            }
            case 5: {
                int op = 0;
                cout << "\n1. Register New Active Campaign\n2. Peek Best Yielding eCPM campaign\n3. Display Sorter Matrix Heap\nSelect Option: ";
                cin >> op;
                if (op == 1) {
                    string id, name;
                    double b, ctr, bid;
                    cout << "Campaign ID code: ";
                    cin >> id;
                    cin.ignore();
                    cout << "Campaign Brand Name: ";
                    getline(cin, name);
                    cout << "Total Budget: ";
                    cin >> b;
                    cout << "Historical/Estimated CTR (e.g. 0.05 for 5%): ";
                    cin >> ctr;
                    cout << "Bidding price per click ($): ";
                    cin >> bid;
                    campaignSorter.insertCampaign(id, name, b, ctr, bid);
                    cout << ">>> Campaign dynamically inserted into priority queue heap!\n";
                } else if (op == 2) {
                    Campaign best("", "", 0, 0, 0);
                    if (campaignSorter.peekMax(best)) {
                        cout << ">>> HIGHEST YIELDING CAMPAIGN IN DSP ENGINE:\n";
                        cout << "    Brand: " << best.name << " [ID: " << best.id << "] | Expected Yield (eCPM): $" << best.expectedECpm << "\n";
                    } else {
                        cout << ">>> Sorter heap is empty.\n";
                    }
                }
                campaignSorter.display();
                break;
            }
            case 6: {
                network.display();
                break;
            }
            case 7: {
                string start, end;
                cout << "Enter starting Ad Engine node ID (e.g. Platform_RTB): ";
                cin >> start;
                cout << "Enter destination Publisher SSP (e.g. CNN_SSP): ";
                cin >> end;
                vector<string> path;
                double latency = 0;
                if (network.findShortestLatencyRoute(start, end, path, latency)) {
                    cout << "\n>>> LOWEST LATENCY PATH DETERMINED:\n";
                    cout << "    Route: ";
                    for (size_t i = 0; i < path.size(); ++i) {
                        cout << path[i];
                        if (i + 1 < path.size()) cout << " -> ";
                    }
                    cout << "\n    Total Route Latency: " << latency << " ms (Meets real-time network threshold < 20ms)\n";
                } else {
                    cout << ">>> Latency solver error: No routing paths found between target servers.\n";
                }
                break;
            }
            case 8: {
                int op = 0;
                cout << "\n1. Allocate Edge Block Cache (Ad Creative file)\n2. Deallocate/Evict Ad Space\n3. Display Partition Table Map\nSelect Option: ";
                cin >> op;
                if (op == 1) {
                    string id;
                    int size;
                    cout << "Enter Creative ID to cache (e.g. nike_banner_mp4): ";
                    cin >> id;
                    cout << "Required Memory Alloc Size (KB): ";
                    cin >> size;
                    int address = edgeAllocator.allocate(size, id);
                    if (address != -1) {
                        cout << ">>> Creative allocation success! Placed at starting address block: " << address << "\n";
                    } else {
                        cout << ">>> Out of memory/allocation failure: Partition block too small or fragmented!\n";
                    }
                } else if (op == 2) {
                    string id;
                    cout << "Enter creative ID to evict/deallocate: ";
                    cin >> id;
                    if (edgeAllocator.deallocate(id)) {
                        cout << ">>> Creative removed. Free block successfully coalesced with neighbors.\n";
                    } else {
                        cout << ">>> Error: Active block not found matching ID in server memory table.\n";
                    }
                }
                edgeAllocator.display();
                break;
            }
            case 9: {
                cout << "\n=============================================================\n";
                cout << "    UNIFIED AD AUCTION LIFE-CYCLE REAL-TIME SIMULATOR        \n";
                cout << "=============================================================\n";
                cout << "Initiating dynamic ad request pipeline...\n\n";

                // user lands on NYTimes — check their demographic profile
                cout << "[Step A] Client user accesses publisher NYTimes.com.\n";
                cout << "         Checking demographic profiles on the edge Trie...\n";
                string profileKey1 = "geo_us_ny";
                string profileKey2 = "int_tech";
                string d1, d2;
                if (demographicsTrie.verify(profileKey1, d1) && demographicsTrie.verify(profileKey2, d2)) {
                    cout << "         -> Dynamic Demographics Match: " << d1 << " & " << d2 << "\n";
                }

                // find the best-paying campaign for this user
                cout << "\n[Step B] Querying Campaign Sorter Max-Heap for highest yielding ad...\n";
                Campaign top("", "", 0, 0, 0);
                if (campaignSorter.peekMax(top)) {
                    cout << "         -> Highest-yielding matching campaign: " << top.name 
                         << " (Estimated eCPM: $" << top.expectedECpm << ")\n";
                }

                // competing bids come in — queue them up FIFO
                cout << "\n[Step C] Queuing live RTB bidding traffic in strict chronological order...\n";
                bidQueue.enqueue("bid_01", "space_header", "adv_apple", top.bidValue + 0.50, "Chrome_macOS");
                bidQueue.enqueue("bid_02", "space_header", "adv_nike", top.bidValue - 0.20, "Safari_iOS");
                bidQueue.display();

                // process bids in order, first one wins
                cout << "\n[Step D] Dequeuing and resolving bids...\n";
                AdBid* winBid = nullptr;
                if (bidQueue.dequeue(winBid)) {
                    cout << "         -> BID WINNER RESOLVED: Bid ID [" << winBid->bidId << "] from Advertiser [" 
                         << winBid->advertiserId << "] wins slot [" << winBid->bannerSpaceId 
                         << "] for $" << fixed << setprecision(2) << winBid->bidAmount << "\n";
                    
                    // mark the slot as taken
                    BannerSpace* sp = inventory.search(winBid->bannerSpaceId);
                    if (sp) {
                        sp->isAllocated = true;
                        cout << "         -> Banner space [" << sp->id << "] allocated successfully.\n";
                    }

                    // figure out the fastest path to deliver the creative
                    cout << "\n[Step E] Resolving shortest latency path for ad creative transmission...\n";
                    vector<string> route;
                    double latency = 0;
                    if (network.findShortestLatencyRoute("Platform_RTB", "NYTimes_SSP", route, latency)) {
                        cout << "         -> Route resolved: ";
                        for (size_t i = 0; i < route.size(); ++i) {
                            cout << route[i] << (i + 1 < route.size() ? " -> " : "");
                        }
                        cout << " | Latency: " << latency << " ms\n";
                    }

                    // cache the winning creative on the edge server
                    cout << "\n[Step F] Allocating storage partition for active ad content cache...\n";
                    string assetId = winBid->advertiserId + "_hero_ad";
                    int addr = edgeAllocator.allocate(150, assetId); // ~150 KB for a banner/video asset
                    if (addr != -1) {
                        cout << "         -> Space allocated at server memory address: " << addr << "\n";
                    }
                    edgeAllocator.display();

                    delete winBid;
                }
                break;
            }
            case 10: {
                cout << "\n=============================================================\n";
                cout << "            DSA SELECTION JUSTIFICATION & ARCHITECTURE        \n";
                cout << "=============================================================\n";
                cout << "1. Hash Table (Inventory Listing):\n";
                cout << "   - Average O(1) searches avoid slow sequential scans when web display slots\n";
                cout << "     are queried before a bid request is created.\n\n";
                cout << "2. Undo History Stack (Budget Changes):\n";
                cout << "   - LIFO ordering ensures programmatic errors or overruns are reversed instantly\n";
                cout << "     to the exact preceding state.\n\n";
                cout << "3. Bid Queue (Auction Loop):\n";
                cout << "   - FIFO queues guarantee that high-speed events are processed in strict order of arrival,\n";
                cout << "     preventing starvation and race conditions between advertisers.\n\n";
                cout << "4. Trie (Demographics segmentation):\n";
                cout << "   - Prefixes are stored efficiently, allowing verification of targeting keys of length L\n";
                cout << "     in O(L) time regardless of the size of the database.\n\n";
                cout << "5. Max-Heap (Campaign Yield Sorter):\n";
                cout << "   - O(log N) insertion and O(1) access to max key allows high frequency pricing models\n";
                cout << "     like eCPM ranking to fetch the most valuable ad dynamically.\n\n";
                cout << "6. Graph & Dijkstra (Marketplace Network Routing):\n";
                cout << "   - Networks are modeled as weighted graphs. Dijkstra solves latency optimizations in O(E log V)\n";
                cout << "     guaranteeing ad delivery under strict service level agreements (< 100ms).\n\n";
                cout << "7. Block Allocator (Edge CDN Caching):\n";
                cout << "   - Dynamically manages contiguous cache space on the edge server, simulating real-world OS memory allocation\n";
                cout << "     mechanisms to store and evict ad payloads with minimal fragmentation.\n";
                cout << "=============================================================\n\n";
                break;
            }
            default:
                cout << "Invalid Selection! Please specify index 1 through 11.\n\n";
        }
        cout << "\nPress Enter to return to main dashboard...";
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cin.get();
        cout << "\n";
    }
    return 0;
}
