# python3

from collections import OrderedDict
import json
import re

class TopicDataPackager:
    """Read processed topic id data files, consolidate them, and output in json."""

    def __init__(self):
        pass


    def read_id_list(self, filename):
        """Read a file of a single column of ids (usually integers) into a list. Return the list."""
        with open(filename, encoding="utf8") as infile:
            ids = []
            for line in infile:
                ids.append(line.strip())
        return ids


    def read_topic_summary(self, filename):
        """Read a topic id summary and return it as a list of dicts."""
        # File looks like:
        # [other stuff]
        # ---- ------ ----- ------ -----  ----------------
        #             Topic    Doc  % of 
        #     # Index Score Purity  Docs  Summary
        #  ---- ------ ----- ------ -----  ----------------
        #     1 (  46)  3.66  0.659  5.55  rural areas banking approximately development con focus quality su services
        #     2 (  16)  2.28  0.715  3.19  microfinance poor families promoting values mission communities sustainable provision follow

        RE_TOPIC = re.compile(r"\s*(\d+)\s+\(\s*(\d+)\)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s+(.*)$")

        with open(filename, encoding="utf8") as infile:
            topics = []
            for line in infile:
                line = line.rstrip()
                match = RE_TOPIC.match(line)
                if match:
                    # Change from 1-based to 0-based.
                    topic_num = int(match.group(1)) - 1
                    index = int(match.group(2))
                    topic_score = float(match.group(3))
                    doc_purity = float(match.group(4))
                    # Originally a percentage.
                    fraction_of_docs = float(match.group(5)) / 100.0
                    summary_words = match.group(6).split()
                    
                    topics.append(OrderedDict((
                                ('topic_num', topic_num), # Redundant, but helps readability.
                                ('topic_score', topic_score),
                                ('doc_purity', doc_purity),
                                ('fraction_of_docs', fraction_of_docs),
                                ('summary_words', summary_words),
                                )))
            return topics
                            

    def read_topic_probabilities(self, filename):
        with open(filename, encoding="utf8") as infile:
            probs = []
            for line in infile:
                probs.append([float(p) for p in line.split()] )
            return probs


    def process(self, document_id_list, topic_list, topic_probabilities, output_filename):
        """document_id_list: a list of document numbers,
        topic_list: a list of the top n topics; each entry is a dict of topic info
        topic_probs: a list of lists of probabilities for each document; the outer list
          is in one-to-one-correspondence with id_list; the inner lists correspond to
          the topic_list entries
        output_filename: where to write the json"""

        data_dict = OrderedDict()
        data_dict['filename'] = output_filename
        data_dict['topics'] = topic_list
        data_dict['document_topics'] = OrderedDict()
        topics = data_dict['document_topics']
        for (doc_id, one_topic_probs) in zip(document_id_list, topic_probabilities):
            # top_topic is index of max probability
            topics[doc_id] = OrderedDict((('top_topic', max(enumerate(one_topic_probs), 
                                                            key=lambda elt: elt[1])[0]), 
                                          ('topic_probabilities', one_topic_probs),
                                          ))
            
        print("Writing {} ...".format(output_filename))
        with open(output_filename, "w") as outfile:
            json.dump(data_dict, outfile, check_circular=False, indent=1, separators=(',', ': '))
